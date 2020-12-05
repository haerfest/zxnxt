#include <SDL2/SDL.h>
#include <stdio.h>
#include "defs.h"
#include "memory.h"
#include "mmu.h"
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
    /* VGA  */ { /* 50 Hz */ { 312, 192, 57, 14, 49 }, /* 50 Hz */ { 262, 192, 33, 14, 23 } },
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
  E_ULA_DISPLAY_STATE_DISPLAY,
  E_ULA_DISPLAY_STATE_RIGHT_BORDER,
  E_ULA_DISPLAY_STATE_HSYNC,
  E_ULA_DISPLAY_STATE_BOTTOM_BORDER,
  E_ULA_DISPLAY_STATE_VSYNC
} ula_display_state_t;


typedef struct {
  SDL_Renderer*           renderer;
  u16_t                   line_offsets[192];
  mmu_bank_t              display_bank;
  u16_t                   display_offset;
  ula_display_frequency_t display_frequency;
  ula_display_state_t     display_state;
  unsigned int            display_line;
  unsigned int            display_column;
  u8_t                    display_byte;
  u8_t                    display_pixel_mask;
  u8_t                    border_colour;
  u8_t                    speaker_state;
} ula_t;


static ula_t ula;


static void ula_state_machine_run(unsigned int delta, const ula_display_spec_t spec) {
  unsigned int tick;

  /* The ULA uses a 7 MHz clock to refresh the display, which is 28/4 MHz. */
  for (tick = 0; tick < delta; tick += 4) {
    switch (ula.display_state) {
      case E_ULA_DISPLAY_STATE_TOP_BORDER:
        SDL_SetRenderDrawColor(ula.renderer, colours[ula.border_colour].red, colours[ula.border_colour].green, colours[ula.border_colour].blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(ula.renderer, ula.display_column, ula.display_line);
        ula.display_column++;
        if (ula.display_column == 32 + 256 + 64) {
          ula.display_state = E_ULA_DISPLAY_STATE_HSYNC;
        }
        break;

      case E_ULA_DISPLAY_STATE_LEFT_BORDER:
        SDL_SetRenderDrawColor(ula.renderer, colours[ula.border_colour].red, colours[ula.border_colour].green, colours[ula.border_colour].blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(ula.renderer, ula.display_column, ula.display_line);
        ula.display_column++;
        if (ula.display_column == 32) {
          ula.display_offset = ula.line_offsets[ula.display_line];
          ula.display_state  = E_ULA_DISPLAY_STATE_DISPLAY;
        }
        break;

      case E_ULA_DISPLAY_STATE_DISPLAY:
        if (ula.display_pixel_mask == 0x00) {
          ula.display_byte = mmu_bank_read(ula.display_bank, ula.display_offset);
          ula.display_offset++;
          ula.display_pixel_mask = 0x80;
        }

        if (ula.display_byte & ula.display_pixel_mask) {
          SDL_SetRenderDrawColor(ula.renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
        } else {
          SDL_SetRenderDrawColor(ula.renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
        }
        SDL_RenderDrawPoint(ula.renderer, ula.display_column, ula.display_line);

        ula.display_pixel_mask >>= 1;
        ula.display_column++;
        if (ula.display_column == 32 + 256) {
          ula.display_state = E_ULA_DISPLAY_STATE_RIGHT_BORDER;
        }
        break;

      case E_ULA_DISPLAY_STATE_HSYNC:
        ula.display_column++;
        if (ula.display_column == 32 + 256 + 64 + 96) {
          ula.display_column = 0;
          ula.display_line++;
          if (ula.display_line < spec.top_border_lines) {
            ula.display_state = E_ULA_DISPLAY_STATE_TOP_BORDER;
          } else if (ula.display_line < spec.top_border_lines + spec.display_lines) {
            ula.display_state = E_ULA_DISPLAY_STATE_LEFT_BORDER;
          } else {
            ula.display_state = E_ULA_DISPLAY_STATE_BOTTOM_BORDER;
          }
        }
        break;

      case E_ULA_DISPLAY_STATE_RIGHT_BORDER:
        SDL_SetRenderDrawColor(ula.renderer, colours[ula.border_colour].red, colours[ula.border_colour].green, colours[ula.border_colour].blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(ula.renderer, ula.display_column, ula.display_line);
        ula.display_column++;
        if (ula.display_column == 32 + 256 + 64) {
          ula.display_state = E_ULA_DISPLAY_STATE_HSYNC;
        }
        break;

      case E_ULA_DISPLAY_STATE_BOTTOM_BORDER:
        SDL_SetRenderDrawColor(ula.renderer, colours[ula.border_colour].red, colours[ula.border_colour].green, colours[ula.border_colour].blue, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawPoint(ula.renderer, ula.display_column, ula.display_line);
        ula.display_column++;
        if (ula.display_column == 32 + 256 + 64) {
          if (ula.display_line < spec.total_lines) {
            ula.display_state = E_ULA_DISPLAY_STATE_HSYNC;
          } else {
            /* TODO: Generate VSYNC interrupt. */
            SDL_RenderPresent(ula.renderer);
            ula.display_state = E_ULA_DISPLAY_STATE_VSYNC;
          }
        }
        break;

      case E_ULA_DISPLAY_STATE_VSYNC:
        ula.display_column++;
        if (ula.display_column == 32 + 256 + 64 + 96) {
          ula.display_column = 0;
          ula.display_line++;
          if (ula.display_line == spec.total_lines + spec.blanking_period_lines) {
            ula.display_line   = 0;
            ula.display_offset = 0;
            ula.display_state  = E_ULA_DISPLAY_STATE_TOP_BORDER;
          }
        }
        break;
    }
  }
}


static void ula_ticks_callback(u64_t ticks, unsigned int delta) {
  const unsigned int       i    = clock_display_timing_get() - E_CLOCK_DISPLAY_TIMING_ZX_48K;
  const unsigned int       j    = clock_video_timing_get() != E_CLOCK_VIDEO_TIMING_HDMI;
  const unsigned int       k    = ula.display_frequency;
  const ula_display_spec_t spec = ula_display_spec[i][j][k];

  ula_state_machine_run(delta, spec);
}


static void ula_fill_tables(void) {
  int line;

  for (line = 0; line < 192; line++) {
    ula.line_offsets[line] = 2048 * line / 64 + (line % 8) * 8 + line / 8;
  }
}


int ula_init(SDL_Renderer* renderer, SDL_Texture* texture) {
  ula.renderer           = renderer;
  ula.display_bank       = 5;
  ula.display_offset     = 0;
  ula.display_frequency  = E_ULA_DISPLAY_FREQUENCY_50HZ;
  ula.display_state      = E_ULA_DISPLAY_STATE_TOP_BORDER;
  ula.display_line       = 0;
  ula.display_column     = 0;
  ula.display_pixel_mask = 0;
  ula.border_colour      = 0;
  ula.speaker_state      = 0;

  ula_fill_tables();

  if (clock_register_callback(ula_ticks_callback) != 0) {
    return -1;
  }

  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  fprintf(stderr, "ula: unimplemented read from %04Xh\n", address);
  return 0xFF;  /* No keys pressed. */
}


void ula_write(u16_t address, u8_t value) {
  ula.border_colour = value & 0x03;
  ula.speaker_state = value & 0x04;
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

  ula.display_frequency = frequency;
  fprintf(stderr, "ula: display frequency set to %s Hz\n", descriptions[frequency]);
}
