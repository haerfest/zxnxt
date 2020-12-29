#include <SDL2/SDL.h>
#include <stdio.h>
#include "defs.h"
#include "memory.h"
#include "ula.h"


/** See: https://wiki.specnext.dev/Reference_machines. */
typedef struct {
  unsigned int total_lines;
  unsigned int display_lines;
  unsigned int bottom_border_lines;
  unsigned int blanking_period_lines;
  unsigned int top_border_lines;
} ula_display_spec_t;


static const ula_display_spec_t ula_display_spec[5][2][2] = {
  /* ZX Spectrum 48K */ {
    /* VGA  */ { /* 50 Hz */ { 312, 192, 57, 14, 49 }, /* 60 Hz */ { 262, 192, 33, 14, 23 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* ZX Spectrum 128K/+2 */ {
    /* VGA  */ { /* 50 Hz */ { 311, 192, 57, 14, 48 }, /* 60 Hz */ { 261, 192, 33, 14, 22 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* ZX Spectrum +2A/+2B/+3 */ {
    /* VGA  */ { /* 50 Hz */ { 311, 192, 57, 14, 48 }, /* 60 Hz */ { 261, 192, 33, 14, 22 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* Pentagon */ {
    /* VGA  */ { /* 50 Hz */ { 320, 192, 49, 14, 65 }, /* 60 Hz */ { 320, 192, 49, 14, 65 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  }
};


typedef struct {
  u8_t red;
  u8_t green;
  u8_t blue;
} ula_colour_t;


static const ula_colour_t colours[] = {
  { 0x00, 0x00, 0x00 },  /*  0: Dark black.     */
  { 0x00, 0x00, 0xD7 },  /*  1: Dark blue.      */
  { 0xD7, 0x00, 0x00 },  /*  2: Dark red.       */
  { 0xD7, 0x00, 0xD7 },  /*  3: Dark magenta.   */
  { 0x00, 0xD7, 0x00 },  /*  4: Dark green.     */
  { 0x00, 0xD7, 0xD7 },  /*  5: Dark cyan.      */
  { 0xD7, 0xD7, 0x00 },  /*  6: Dark yellow.    */
  { 0xD7, 0xD7, 0xD7 },  /*  7: Dark white.     */
  { 0x00, 0x00, 0x00 },  /*  8: Bright black.   */
  { 0x00, 0x00, 0xFF },  /*  9: Bright blue.    */
  { 0xFF, 0x00, 0x00 },  /* 10: Bright red.     */
  { 0xFF, 0x00, 0xFF },  /* 11: Bright magenta. */
  { 0x00, 0xFF, 0x00 },  /* 12: Bright green.   */
  { 0x00, 0xFF, 0xFF },  /* 13: Bright cyan.    */
  { 0xFF, 0xFF, 0x00 },  /* 14: Bright yellow.  */
  { 0xFF, 0xFF, 0xFF }   /* 15: Bright white.   */
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
  SDL_Renderer*           renderer;
  u16_t                   display_offsets[192];
  u8_t*                   display_ram;
  u16_t                   display_offset;
  u8_t                    display_byte;
  u8_t                    display_pixel_mask;
  ula_display_frequency_t display_frequency;
  ula_display_state_t     display_state;
  unsigned int            display_line;
  unsigned int            display_column;  
  u16_t                   attribute_offsets[192];
  u8_t*                   attribute_ram;
  u16_t                   attribute_offset;
  u8_t                    attribute_byte;
  u8_t                    border_colour;
  u8_t                    speaker_state;
} ula_t;


static ula_t self;


static void ula_state_machine_run(unsigned int delta, const ula_display_spec_t spec) {
  unsigned int tick;
  ula_colour_t colour;

  /* The ULA uses a 7 MHz clock to refresh the display, which is 28/4 MHz. */
  for (tick = 0; tick < delta; tick += 4) {
    switch (self.display_state) {
      case E_ULA_DISPLAY_STATE_TOP_BORDER:
        colour = colours[self.border_colour];
        SDL_SetRenderDrawColor(self.renderer, colour.red, colour.green, colour.blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(self.renderer, self.display_column, self.display_line);
        self.display_column++;
        if (self.display_column == 32 + 256 + 64) {
          self.display_state = E_ULA_DISPLAY_STATE_HSYNC;
        }
        break;

      case E_ULA_DISPLAY_STATE_LEFT_BORDER:
        colour = colours[self.border_colour];
        SDL_SetRenderDrawColor(self.renderer, colour.red, colour.green, colour.blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(self.renderer, self.display_column, self.display_line);
        self.display_column++;
        if (self.display_column == 32) {
          const u16_t line = self.display_line - spec.top_border_lines;
          self.display_offset   = self.display_offsets[line];
          self.attribute_offset = self.attribute_offsets[line];
          self.display_state    = E_ULA_DISPLAY_STATE_FRAME_BUFFER;
        }
        break;

      case E_ULA_DISPLAY_STATE_FRAME_BUFFER:
        if (self.display_pixel_mask == 0x00) {
          self.display_pixel_mask = 0x80;
          self.display_byte       = self.display_ram[self.display_offset++];
          self.attribute_byte     = self.attribute_ram[self.attribute_offset++];
        }

        /* TODO: blinking colours. */
        colour = colours[
          (self.display_byte & self.display_pixel_mask ? self.attribute_byte : self.attribute_byte >> 3) & 0x07
          + (self.attribute_byte & 0x20) * 7
        ];
        
        SDL_SetRenderDrawColor(self.renderer, colour.red, colour.green, colour.blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(self.renderer, self.display_column, self.display_line);

        self.display_pixel_mask >>= 1;
        self.display_column++;
        if (self.display_column == 32 + 256) {
          self.display_state = E_ULA_DISPLAY_STATE_RIGHT_BORDER;
        }
        break;

      case E_ULA_DISPLAY_STATE_HSYNC:
        self.display_column++;
        if (self.display_column == 32 + 256 + 64 + 96) {
          self.display_column = 0;
          self.display_line++;
          if (self.display_line < spec.top_border_lines) {
            self.display_state = E_ULA_DISPLAY_STATE_TOP_BORDER;
          } else if (self.display_line < spec.top_border_lines + spec.display_lines) {
            self.display_state = E_ULA_DISPLAY_STATE_LEFT_BORDER;
          } else {
            self.display_state = E_ULA_DISPLAY_STATE_BOTTOM_BORDER;
          }
        }
        break;

      case E_ULA_DISPLAY_STATE_RIGHT_BORDER:
        colour = colours[self.border_colour];
        SDL_SetRenderDrawColor(self.renderer, colour.red, colour.green, colour.blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(self.renderer, self.display_column, self.display_line);
        self.display_column++;
        if (self.display_column == 32 + 256 + 64) {
          self.display_state = E_ULA_DISPLAY_STATE_HSYNC;
        }
        break;

      case E_ULA_DISPLAY_STATE_BOTTOM_BORDER:
        colour = colours[self.border_colour];
        SDL_SetRenderDrawColor(self.renderer, colour.red, colour.green, colour.blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(self.renderer, self.display_column, self.display_line);
        self.display_column++;
        if (self.display_column == 32 + 256 + 64) {
          if (self.display_line < spec.total_lines) {
            self.display_state = E_ULA_DISPLAY_STATE_HSYNC;
          } else {
            /* TODO: Generate VSYNC interrupt. */
            self.display_state = E_ULA_DISPLAY_STATE_VSYNC;
            SDL_RenderPresent(self.renderer);
          }
        }
        break;

      case E_ULA_DISPLAY_STATE_VSYNC:
        self.display_column++;
        if (self.display_column == 32 + 256 + 64 + 96) {
          self.display_column = 0;
          self.display_line++;
          if (self.display_line == spec.total_lines + spec.blanking_period_lines) {
            self.display_line     = 0;
            self.display_offset   = 0;
            self.attribute_offset = 0;
            self.display_state    = E_ULA_DISPLAY_STATE_TOP_BORDER;
          }
        }
        break;
    }
  }
}


static void ula_ticks_callback(u64_t ticks, unsigned int delta) {
  const unsigned int       i    = clock_display_timing_get() - E_CLOCK_DISPLAY_TIMING_ZX_48K;
  const unsigned int       j    = clock_video_timing_get() != E_CLOCK_VIDEO_TIMING_HDMI;
  const unsigned int       k    = self.display_frequency;
  const ula_display_spec_t spec = ula_display_spec[i][j][k];

  ula_state_machine_run(delta, spec);
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
  self.renderer           = renderer;
  self.display_ram        = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];  /* Always bank 5. */
  self.attribute_ram      = &self.display_ram[192 * 32];
  self.display_offset     = 0;
  self.attribute_offset   = 0;
  self.display_frequency  = E_ULA_DISPLAY_FREQUENCY_50HZ;
  self.display_state      = E_ULA_DISPLAY_STATE_TOP_BORDER;
  self.display_line       = 0;
  self.display_column     = 0;
  self.display_pixel_mask = 0;
  self.border_colour      = 0;
  self.speaker_state      = 0;

  ula_fill_tables();

  if (clock_register_callback(ula_ticks_callback) != 0) {
    return -1;
  }

  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  fprintf(stderr, "ula: unimplemented read from $%04X\n", address);
  return 0x1F;  /* No keys pressed. */
}


void ula_write(u16_t address, u8_t value) {
  self.border_colour = value & 0x03;
  self.speaker_state = value & 0x04;
}


void ula_display_timing_set(clock_display_timing_t timing) {
  clock_display_timing_set(timing);
}


void ula_video_timing_set(clock_video_timing_t timing) {
  clock_video_timing_set(timing);
}


void ula_display_frequency_set(ula_display_frequency_t frequency) {
  const char* descriptions[] = {
    "50",
    "60"
  };

  self.display_frequency = frequency;
  fprintf(stderr, "ula: display frequency set to %s Hz\n", descriptions[frequency]);

  self.display_state      = E_ULA_DISPLAY_STATE_TOP_BORDER;
  self.display_line       = 0;
  self.display_column     = 0;
  self.display_pixel_mask = 0;
}
