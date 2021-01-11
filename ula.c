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


#define PALETTE_OFFSET_INK      0
#define PALETTE_OFFSET_PAPER   16
#define PALETTE_OFFSET_BORDER  16


typedef enum {
  E_ULA_DISPLAY_MODE_SCREEN_0  = 0x00,
  E_ULA_DISPLAY_MODE_SCREEN_1  = 0x01,
  E_ULA_DISPLAY_MODE_HI_COLOUR = 0x02,
  E_ULA_DISPLAY_MODE_HI_RES    = 0x06
} ula_display_mode_t;


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


static const SDL_Rect ula_source_rect = {
  .x = 0,
  .y = 17,
  .w = WINDOW_WIDTH,
  .h = WINDOW_HEIGHT / 2
};


typedef void (*ula_display_mode_handler)(void);


typedef struct {
  SDL_Renderer*              renderer;
  SDL_Texture*               texture;
  u8_t*                      sram;
  const ula_display_spec_t*  display_spec;
  u16_t                      display_offsets[192];
  u8_t*                      display_ram;
  u8_t*                      display_ram_odd;
  u16_t                      display_offset;
  u8_t                       display_byte;
  u8_t                       display_pixel_mask;
  ula_display_timing_t       display_timing;
  int                        display_frequency;
  ula_display_mode_handler   display_mode_handler;
  unsigned int               display_line;
  unsigned int               display_column;  
  u16_t                      attribute_offsets[192];
  u8_t*                      attribute_ram;
  u16_t                      attribute_offset;
  u8_t                       attribute_byte;
  u8_t                       border_colour;
  u8_t                       speaker_state;
  palette_t                  palette;
  u16_t*                     frame_buffer;
  u16_t*                     pixel;
  int                        clip_x1;
  int                        clip_x2;
  int                        clip_y1;
  int                        clip_y2;
  uint64_t                   frame_counter;
  int                        blink_state;
  s8_t                       audio_last_sample;
  ula_screen_bank_t          screen_bank;
  int                        timex_disable_ula_interrupt;
  ula_display_mode_t         display_mode;
  u8_t                       hi_res_ink_colour;
} self_t;


static self_t self;


static void ula_blit(void) {
  void* pixels;
  int   pitch;

  if (SDL_LockTexture(self.texture, NULL, &pixels, &pitch) != 0) {
    log_err("ula: SDL_LockTexture error: %s\n", SDL_GetError());
    return;
  }

  memcpy(pixels, self.frame_buffer, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH * 2);
  SDL_UnlockTexture(self.texture);

  if (SDL_RenderCopy(self.renderer, self.texture, &ula_source_rect, NULL) != 0) {
    log_err("ula: SDL_RenderCopy error: %s\n", SDL_GetError());
    return;
  }

  SDL_RenderPresent(self.renderer);
}


static void ula_frame_complete(void) {
  if ((++self.frame_counter & 15) == 0) {
    self.blink_state ^= 1;
  }

  /* Start drawing at the top again. */
  self.pixel = self.frame_buffer;

  ula_blit();

  if (!self.timex_disable_ula_interrupt) {
    cpu_irq(32);
  }
}


#include "ula_mode_x.c"
#include "ula_hi_res.c"


static void ula_display_restart(void) {
  self.display_spec         = &ula_display_spec[self.display_timing][self.display_frequency == 60];
  self.display_line         = 0;
  self.display_column       = 0;
  self.display_pixel_mask   = 0;
  self.frame_counter        = 0;
  self.blink_state          = 0;
  self.pixel                = self.frame_buffer;

  switch (self.display_mode) {
    case E_ULA_DISPLAY_MODE_SCREEN_0:
    case E_ULA_DISPLAY_MODE_SCREEN_1:
      self.display_mode_handler = ula_display_mode_screen_x_top_border;
      break;

    case E_ULA_DISPLAY_MODE_HI_RES:
      self.display_mode_handler = ula_display_mode_hi_res_top_border;
      break;

    case E_ULA_DISPLAY_MODE_HI_COLOUR:
      log_wrn("ula: hi-colour mode not implemented\n");
      break;
  }
}


/* Runs the ULA for 'ticks_14mhz', i.e. ticks of the ULA's 14 MHz clock. */
void ula_run(u32_t ticks_14mhz) {
  const u32_t divider = (self.display_mode == E_ULA_DISPLAY_MODE_HI_RES ? 1 : 2);
  u32_t       tick;

  for (tick = 0; tick < ticks_14mhz; tick += divider) {
    self.display_mode_handler();
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


static void ula_set_display_mode(ula_display_mode_t mode) {
  self.display_mode = mode;
  
  switch (self.display_mode) {
    case E_ULA_DISPLAY_MODE_SCREEN_0:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.attribute_ram = &self.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_SCREEN_1:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024 + 0x2000];
      self.attribute_ram = &self.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_COLOUR:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.attribute_ram = &self.display_ram[0x2000];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_RES:
      self.display_ram     = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.display_ram_odd = &self.display_ram[0x2000];
      break;

    default:
      log_wrn("ula: invalid display mode %d\n", mode);
      return;
  }

  ula_display_restart();
}


int ula_init(SDL_Renderer* renderer, SDL_Texture* texture, u8_t* sram) {
  self.frame_buffer = malloc(FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH * 2);
  if (self.frame_buffer == NULL) {
    log_err("ula: out of memory\n");
    return -1;
  }

  self.renderer                    = renderer;
  self.texture                     = texture;
  self.sram                        = sram;
  self.screen_bank                 = E_ULA_SCREEN_BANK_5;
  self.display_offset              = 0;
  self.attribute_offset            = 0;
  self.display_timing              = E_ULA_DISPLAY_TIMING_ZX_48K;
  self.display_frequency           = 50;
  self.border_colour               = 0;
  self.speaker_state               = 0;
  self.palette                     = E_PALETTE_ULA_FIRST;
  self.clip_x1                     = 0;
  self.clip_x2                     = 255 * 2;
  self.clip_y1                     = 0;
  self.clip_y2                     = 191;
  self.audio_last_sample           = 0;
  self.timex_disable_ula_interrupt = 0;
  self.hi_res_ink_colour           = 0;

  ula_fill_tables();
  ula_set_display_mode(E_ULA_DISPLAY_MODE_SCREEN_0);
  ula_display_restart();

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

  audio_add_sample(E_AUDIO_SOURCE_BEEPER, sample);

  self.speaker_state = speaker_state;
  self.border_colour = value & 0x07;
}


u8_t ula_timex_read(u16_t address) {
  return self.timex_disable_ula_interrupt << 6 |
         self.hi_res_ink_colour           << 3 |
         self.display_mode;
}


void ula_timex_write(u16_t address, u8_t value) {
  self.timex_disable_ula_interrupt = (value & 0x40) >> 6;
  self.hi_res_ink_colour           = (value & 0x38) >> 3;

  ula_set_display_mode(value & 0x07);
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

  ula_display_restart();
}


void ula_display_frequency_set(int is_60hz) {
  const int display_frequency = is_60hz ? 60 : 50;

  if (display_frequency != self.display_frequency) {
    self.display_frequency = display_frequency;
    log_dbg("ula: display frequency set to %d Hz\n", self.display_frequency);

    ula_display_restart();
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


void ula_screen_bank_set(ula_screen_bank_t bank) {
  self.screen_bank = bank;
  self.display_ram = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
}


ula_screen_bank_t ula_screen_bank_get(void) {
  return self.screen_bank;
}
