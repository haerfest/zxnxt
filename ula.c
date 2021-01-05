#include <SDL2/SDL.h>
#include <time.h>
#include "audio.h"
#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "palette.h"
#include "ula.h"


/* For fixed-point low-pass audio filter. */
#define FIXPOINT  4
#define BETA      4


#define PALETTE_OFFSET_INK      0
#define PALETTE_OFFSET_PAPER   16
#define PALETTE_OFFSET_BORDER  16


/** See: https://wiki.specnext.dev/Reference_machines. */
typedef struct {
  unsigned int total_lines;
  unsigned int display_lines;
  unsigned int bottom_border_lines;
  unsigned int blanking_period_lines;
  unsigned int top_border_lines;
} ula_display_spec_t;


#define N_TIMINGS      (E_ULA_DISPLAY_TIMING_PENTAGON - E_ULA_DISPLAY_TIMING_INTERNAL_USE + 1)
#define N_FREQUENCIES  2

static const ula_display_spec_t ula_display_spec[N_TIMINGS][N_FREQUENCIES] = {
  /* Internal use. */ {
    /* 50 Hz */ {   0,   0,  0,  0,  0 },
    /* 60 Hz */ {   0,   0,  0,  0,  0 }
  },
  /* ZX Spectrum 48K */ {
    /* 50 Hz */ { 312, 192, 57, 14, 49 },
    /* 60 Hz */ { 262, 192, 33, 14, 23 }
  },
  /* ZX Spectrum 128K/+2 */ {
    /* 50 Hz */ { 311, 192, 57, 14, 48 },
    /* 60 Hz */ { 261, 192, 33, 14, 22 }
  },
  /* ZX Spectrum +2A/+2B/+3 */ {
    /* 50 Hz */ { 311, 192, 57, 14, 48 },
    /* 60 Hz */ { 261, 192, 33, 14, 22 }
  },
  /* Pentagon */ {
    /* 50 Hz */ { 320, 192, 49, 14, 65 },
    /* 60 Hz */ { 320, 192, 49, 14, 65 }
  }
};


typedef enum {
  E_ULA_DISPLAY_STATE_TOP_BORDER = 0,
  E_ULA_DISPLAY_STATE_LEFT_BORDER,
  E_ULA_DISPLAY_STATE_FRAME_BUFFER,
  E_ULA_DISPLAY_STATE_RIGHT_BORDER,
  E_ULA_DISPLAY_STATE_HSYNC,
  E_ULA_DISPLAY_STATE_BOTTOM_BORDER,
  E_ULA_DISPLAY_STATE_VSYNC
} ula_display_state_t;


typedef struct {
  SDL_Renderer*             renderer;
  SDL_Texture*              texture;
  const ula_display_spec_t* display_spec;
  u16_t                     display_offsets[192];
  u8_t*                     display_ram;
  u16_t                     display_offset;
  u8_t                      display_byte;
  u8_t                      display_pixel_mask;
  ula_display_timing_t      display_timing;
  int                       display_frequency;
  ula_display_state_t       display_state;
  unsigned int              display_line;
  unsigned int              display_column;  
  u16_t                     attribute_offsets[192];
  u8_t*                     attribute_ram;
  u16_t                     attribute_offset;
  u8_t                      attribute_byte;
  u8_t                      border_colour;
  u8_t                      speaker_state;
  palette_t                 palette;
  u8_t*                     frame_buffer;
  u8_t*                     pixel;
  int                       clip_x1;
  int                       clip_x2;
  int                       clip_y1;
  int                       clip_y2;
  int                       frame_counter;
  int                       blink_state;
  s16_t                     audio_last_sample_lpf;
  s8_t                      audio_last_sample;
} self_t;


#define PLOT(colour) \
  *self.pixel++ = colour.blue << 4 | 0; \
  *self.pixel++ = colour.red  << 4 | colour.green;


static self_t self;


static void ula_reset_display_spec(void) {
  self.display_spec       = &ula_display_spec[self.display_timing][self.display_frequency == 60];
  self.display_state      = E_ULA_DISPLAY_STATE_TOP_BORDER;
  self.display_line       = 0;
  self.display_column     = 0;
  self.display_pixel_mask = 0;
  self.pixel              = self.frame_buffer;
  self.frame_counter      = 0;
  self.blink_state        = 0;
}


static void ula_blit(void) {
  void* pixels;
  int   pitch;

  if (SDL_LockTexture(self.texture, NULL, &pixels, &pitch) != 0) {
    log_err("ula: SDL_LockTexture error: %s\n", SDL_GetError());
    return;
  }

  memcpy(pixels, self.frame_buffer, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH * 2);
  SDL_UnlockTexture(self.texture);

  if (SDL_RenderCopy(self.renderer, self.texture, NULL, NULL) != 0) {
    log_err("ula: SDL_RenderCopy error: %s\n", SDL_GetError());
    return;
  }

  SDL_RenderPresent(self.renderer);
}


static void ula_display_state_top_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  PLOT(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_state = E_ULA_DISPLAY_STATE_HSYNC;
  }
}


static void ula_display_state_left_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  PLOT(colour);

  if (++self.display_column == 32) {
    const u16_t line = self.display_line - self.display_spec->top_border_lines;
    self.display_offset   = self.display_offsets[line];
    self.attribute_offset = self.attribute_offsets[line];
    self.display_state    = E_ULA_DISPLAY_STATE_FRAME_BUFFER;
  }
}


static void ula_display_state_frame_buffer(void) {
  palette_entry_t colour;
  u8_t            index;
  
  if (self.display_pixel_mask == 0x00) {
    self.display_pixel_mask = 0x80;
    self.display_byte       = self.display_ram[self.display_offset++];
    self.attribute_byte     = self.attribute_ram[self.attribute_offset++];
  }

  if (self.display_line   >= self.display_spec->top_border_lines + self.clip_y1 &&
      self.display_line   <= self.display_spec->top_border_lines + self.clip_y2 &&
      self.display_column >= 32 + self.clip_x1 &&
      self.display_column <= 32 + self.clip_x2) {

    const u8_t ink   = PALETTE_OFFSET_INK   + (self.attribute_byte & 0x07);
    const u8_t paper = PALETTE_OFFSET_PAPER + (self.attribute_byte >> 3) & 0x07;
    const int  blink = self.attribute_byte & 0x80;
 
    if (self.display_byte & self.display_pixel_mask) {
      index = blink && self.blink_state ? paper : ink;
    } else {
      index = blink && self.blink_state ? ink : paper;
    }
    if (self.attribute_byte & 0x40) {
      /* Bright. */
      index += 8;
    }
  } else {
    index = PALETTE_OFFSET_BORDER + self.border_colour;
  }

  colour = palette_read_rgb(self.palette, index);
  PLOT(colour);

  self.display_pixel_mask >>= 1;
  if (++self.display_column == 32 + 256) {
    self.display_state = E_ULA_DISPLAY_STATE_RIGHT_BORDER;
  }
}


static void ula_display_state_hsync(void) {
  if (++self.display_column == 32 + 256 + 64 + 96) {
    self.display_column = 0;
    self.display_line++;
    self.display_state = (self.display_line < self.display_spec->top_border_lines)                                    ? E_ULA_DISPLAY_STATE_TOP_BORDER
                       : (self.display_line < self.display_spec->top_border_lines + self.display_spec->display_lines) ? E_ULA_DISPLAY_STATE_LEFT_BORDER
                       : E_ULA_DISPLAY_STATE_BOTTOM_BORDER;
  }
}


static void ula_display_state_right_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  PLOT(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_state = E_ULA_DISPLAY_STATE_HSYNC;
  }
}


/**
 * https://wiki.specnext.dev/Reference_machines
 *
 * "The ULA VBLANK Interrupt is the point at which the ULA sends an interrupt
 * to the Z80 causing it to wake from HALT instructions and run the frame
 * service routine. On all non-HDMI machines, this happens immediately before
 * the blanking period begins, except for Pentagon where it happens 1 line
 * earlier. On HDMI, because of the longer blanking period, it happens during
 * the blanking period rather than before. At 50Hz, it happens 25 lines into
 * the blanking period, leaving 15 lines between the interrupt and the actual
 * start of the top border (close to the 14 lines it would be on VGA output).
 * At 60Hz, it happens 24 lines into blanking, leaving only 9 lines."
 */
static void ula_display_state_bottom_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  PLOT(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_state = (self.display_line < self.display_spec->total_lines)
                       ? E_ULA_DISPLAY_STATE_HSYNC
                       : E_ULA_DISPLAY_STATE_VSYNC;

    if (self.display_state == E_ULA_DISPLAY_STATE_VSYNC) {
      self.frame_counter++;
      if (self.frame_counter == self.display_frequency / 2) {
        self.blink_state ^= 1;
      } else if (self.frame_counter == self.display_frequency) {
        self.blink_state ^= 1;
        self.frame_counter = 0;
      }

      ula_blit();
      cpu_irq(32);
    }
  }
}


static void ula_display_state_vsync(void) {
  if (++self.display_column == 32 + 256 + 64 + 96) {
    self.display_column = 0;
    self.display_line++;

    if (self.display_line == self.display_spec->total_lines + self.display_spec->blanking_period_lines) {
      self.display_line       = 0;
      self.display_offset     = 0;
      self.attribute_offset   = 0;
      self.display_state      = E_ULA_DISPLAY_STATE_TOP_BORDER;
      self.pixel              = self.frame_buffer;
    }
  }
}


void ula_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    switch (self.display_state) {
      case E_ULA_DISPLAY_STATE_TOP_BORDER:
        ula_display_state_top_border();
        break;

      case E_ULA_DISPLAY_STATE_LEFT_BORDER:
        ula_display_state_left_border();
        break;

      case E_ULA_DISPLAY_STATE_FRAME_BUFFER:
        ula_display_state_frame_buffer();
        break;

      case E_ULA_DISPLAY_STATE_HSYNC:
        ula_display_state_hsync();
        break;

      case E_ULA_DISPLAY_STATE_RIGHT_BORDER:
        ula_display_state_right_border();
        break;

      case E_ULA_DISPLAY_STATE_BOTTOM_BORDER:
        ula_display_state_bottom_border();
        break;

      case E_ULA_DISPLAY_STATE_VSYNC:
        ula_display_state_vsync();
        break;
    }
  }
}


static void ula_fill_tables(void) {
  int line;

  for (line = 0; line < 192; line++) {
    const u8_t third         = line / 64;                /* Which third of the screen the line falls in: 0, 1 or 2. */
    const u8_t line_rel      = line - third * 64;        /* Relative line within the third: 0 .. 63. */
    const u8_t char_row      = line_rel / 8;             /* Which character row the line falls in: 0 .. 7. */
    const u8_t char_row_line = line_rel - char_row * 8;  /* Which line within character row: 0 .. 7. */

    self.display_offsets[line]   = third * 2048 + (char_row_line * 8 + char_row) * 32;
    self.attribute_offsets[line] = (line / 8) * 32;
  }
}


int ula_init(SDL_Renderer* renderer, SDL_Texture* texture, u8_t* sram) {
  self.frame_buffer = malloc(FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH * 2);
  if (self.frame_buffer == NULL) {
    log_err("ula: out of memory\n");
    return -1;
  }

  self.renderer          = renderer;
  self.texture           = texture;
  self.display_ram       = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];  /* Always bank 5. */
  self.attribute_ram     = &self.display_ram[192 * 32];
  self.display_offset    = 0;
  self.attribute_offset  = 0;
  self.display_timing    = E_ULA_DISPLAY_TIMING_ZX_48K;
  self.display_frequency = 50;
  self.border_colour     = 0;
  self.speaker_state     = 0;
  self.palette           = E_PALETTE_ULA_FIRST;
  self.clip_x1           = 0;
  self.clip_x2           = 255 * 2;
  self.clip_y1           = 0;
  self.clip_y2           = 191;

  ula_fill_tables();
  ula_reset_display_spec();

  self.audio_last_sample_lpf            = 0;
  self.audio_last_sample                = 0;

  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  return keyboard_read(address);
}


void ula_write(u16_t address, u8_t value) {
  const u8_t speaker_state = value & 0x10;
  const s8_t sample        = speaker_state ? 127 : -128;

  /* Apply a low-pass filter to mimick the physical properties of the real
   * speaker. */
  self.audio_last_sample_lpf = (((self.audio_last_sample_lpf << BETA) - self.audio_last_sample_lpf) + ((s16_t) sample << FIXPOINT)) >> BETA;
  audio_add_sample(self.audio_last_sample_lpf >> FIXPOINT);

  self.speaker_state = speaker_state;
  self.border_colour = value & 0x07;
}


ula_display_timing_t ula_display_timing_get(void) {
  return self.display_timing;
}


void ula_display_timing_set(ula_display_timing_t timing) {
#ifdef DEBUG
  const char* descriptions[] = {
    "internal use",
    "ZX Spectrum 48K",
    "ZX Spectrum 128K/+2",
    "ZX Spectrum +2A/+2B/+3",
    "Pentagon",
    "invalid (5)",
    "invalid (6)",
    "invalid (7)"
  };
#endif

  self.display_timing = timing;
  log_dbg("ula: display timing set to %s\n", descriptions[timing]);

  ula_reset_display_spec();
}


void ula_display_frequency_set(int is_60hz) {
  const int display_frequency = is_60hz ? 60 : 50;

  if (display_frequency != self.display_frequency) {
    self.display_frequency = display_frequency;
    log_dbg("ula: display frequency set to %d Hz\n", self.display_frequency);

    ula_reset_display_spec();
  }
}


void ula_palette_set(int use_second) {
  self.palette = use_second ? E_PALETTE_ULA_SECOND : E_PALETTE_ULA_FIRST;
}


void ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;

  log_dbg("ula: clipping window set to %d <= x <= %d and %d <= y <= %d\n", self.clip_x1, self.clip_x2, self.clip_y1, self.clip_y2);
}
