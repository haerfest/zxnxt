#include <SDL2/SDL.h>
#include <string.h>
#include "copper.h"
#include "cpu.h"
#include "defs.h"
#include "layer2.h"
#include "log.h"
#include "palette.h"
#include "slu.h"
#include "sprites.h"
#include "tilemap.h"
#include "ula.h"


/**
 * A standard ULA screen (PAL) is generated like this, with pixels horizontally
 * and lines vertically:
 *
 * +-------------------------------------+
 * |                  ^                  |
 * |                 56                  |
 * |                  v                  |
 * |        +-------------------+        |
 * |        |                   |        |
 * |        |               ^   |        |
 * | < 32 > | < 256 >      192  | < 64 > | < 96 >
 * |        |               v   |        |
 * |        |                   |        |
 * |        +-------------------+        |
 * |                  ^                  |
 * |                 56                  |
 * |                  v                  |
 * +-------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * In terms of timing, a single horizontal line takes:
 *
 *   32 + 256 + 64 + 96 = 448 pixels.
 *
 * And a single screen takes:
 *
 *   56 + 192 + 56 + 8 = 312 lines.
 *
 * At a 7 MHz pixel-clock, that gives 312 * 448 / 7 MHz = 19.968 msec
 * per frame, or a frame rate of 50.08 Hz.
 *
 * The Next uses a 14 MHz pixel-clock since it has video modes that double the
 * horizontal resolution. For a high-resolution ULA screen (PAL) which has a
 * content area 512 pixels wide, that becomes:
 *
 * +--------------------------------------+
 * |                  ^                   |
 * |                 56                   |
 * |                  v                   |
 * |        +-------------------+         |
 * |        |                   |         |
 * |        |               ^   |         |
 * | < 64 > | < 512 >      192  | < 128 > | < 192 >
 * |        |               v   |         |
 * |        |                   |         |
 * |        +-------------------+         |
 * |                  ^                   |
 * |                 56                   |
 * |                  v                   |
 * +--------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * And for some of the other video modes the same total width, but slightly
 * different arrangement between borders and their content area of 640 pixels:
 *
 * +----------------------------+
 * |         ^                  |
 * |        56                  |
 * |         v                  |
 * +-------------------+        |
 * |                   |        |
 * |               ^   |        |
 * | < 640 >      192  | < 64 > | < 192 >
 * |               v   |        |
 * |                   |        |
 * +-------------------+        |
 * |         ^                  |
 * |        56                  |
 * |         v                  |
 * +----------------------------+
 *           ^
 *           8
 *           v
 *
 * These "layers" can be overlaid on top of one another. The resulting image
 * that is visible is 640 pixels x 256 lines, which is cut out as follows:
 *
 * For the default ULA screen:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                 32                  |        |
 * |        +-------------------+        |        |
 * |        |                   |        |        |
 * |        |               ^   |        |        |
 * | < 32 > | < 256 >      192  | < 32 > | < 32 > | < 96 >
 * |        |               v   |        |        |
 * |        |                   |        |        |
 * |        +-------------------+        |        |
 * |                 32                  |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * For the high resolution ULA screen:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                 32                  |        |
 * |        +-------------------+        |        |
 * |        |                   |        |        |
 * |        |               ^   |        |        |
 * | < 64 > | < 512 >      192  | < 64 > | < 64 > | < 192 >
 * |        |               v   |        |        |
 * |        |                   |        |        |
 * |        +-------------------+        |        |
 * |                 32                  |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * For the other video modes:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                                     |        |
 * |                                     |        |
 * |                                     |        |
 * |                                 ^   |        |
 * | < 640 >                        256  | < 64 > | < 192 >
 * |                                 v   |        |
 * |                                     |        |
 * |                                     |        |
 * |                                     |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * The SLU blends all the layers together, so we need every layer to fill a
 * 640 x 256 frame buffer and return that so that the SLU can blend them.
 */


#define HORIZONTAL_RETRACE  192  /* Pixels or clock ticks. */
#define VERTICAL_RETRACE      8  /* Lines.                 */


typedef struct {
  SDL_Renderer*        renderer;
  SDL_Texture*         texture;
  u16_t*               frame_buffer;
  slu_layer_priority_t layer_priority;
  u32_t                beam_row;
  u32_t                beam_column;
  int                  is_beam_visible;
  u16_t                fallback_colour;
  int                  line_irq_enabled;
  u16_t                line_irq_row;
} self_t;


static self_t self;


int slu_init(SDL_Renderer* renderer, SDL_Texture* texture) {
  memset(&self, 0, sizeof(self));

  self.frame_buffer = malloc(FRAME_BUFFER_SIZE);
  if (self.frame_buffer == NULL) {
    log_err("slu: out of memory\n");
    return -1;
  }

  self.renderer = renderer;
  self.texture  = texture;

  return 0;
}


void slu_finit(void) {
  if (self.frame_buffer != NULL) {
    free(self.frame_buffer);
    self.frame_buffer = NULL;
  }
}


static void slu_blit(void) {
  SDL_Rect source_rect = {
    .x = 0,
    .y = 0,
    .w = WINDOW_WIDTH,
    .h = WINDOW_HEIGHT / 2
  };
  void* pixels;
  int   pitch;

  if (SDL_LockTexture(self.texture, NULL, &pixels, &pitch) != 0) {
    log_err("slu: SDL_LockTexture error: %s\n", SDL_GetError());
    return;
  }

  memcpy(pixels, self.frame_buffer, FRAME_BUFFER_SIZE);
  SDL_UnlockTexture(self.texture);

  if (SDL_RenderCopy(self.renderer, self.texture, &source_rect, NULL) != 0) {
    log_err("slu: SDL_RenderCopy error: %s\n", SDL_GetError());
    return;
  }

  SDL_RenderPresent(self.renderer);
}


static void slu_beam_advance(void) {
  u16_t rows;
  u16_t columns;

  /**
   * Beam (0, 0) is the top-left pixel of the (typically) 256x192 content
   * area.
   */
  ula_display_size_get(&rows, &columns);

  /* Advance beam one pixel horizontally. */
  if (++self.beam_column < columns) {
    return;
  }

  /* Advance beam to beginning of next line. */
  self.beam_column = 0;
  if (++self.beam_row < rows) {
    return;
  }

  /* Move beam to top left of display. */
  self.beam_row = 0;

  /* Update display. */
  slu_blit();

  /* Notify the ULA that we completed a frame. */
  ula_did_complete_frame();  
}


u32_t slu_run(u32_t ticks_14mhz) {
  u32_t tick;
  u32_t frame_buffer_row;
  u32_t frame_buffer_column;

  int   ula_is_transparent;
  u16_t ula_rgba;

  int   tilemap_is_transparent;
  u16_t tilemap_rgba;

  int   sprites_is_transparent;
  u16_t sprites_rgba;

  int   ula_tilemap_is_transparent;
  u16_t ula_tilemap_rgba;

  int   layer2_is_transparent;
  u16_t layer2_rgba;
  int   layer2_is_priority;

  u16_t rgba;

  for (tick = 0; tick < ticks_14mhz; tick++) {
    slu_beam_advance();

    if (self.line_irq_enabled && self.beam_column == 0 && self.beam_row == self.line_irq_row) {
      cpu_irq(32);
    }

    /* Copper runs at 28 MHz. */
    copper_tick(self.beam_row, self.beam_column);
    copper_tick(self.beam_row, self.beam_column);

    if (!ula_tick(self.beam_row, self.beam_column, &ula_is_transparent, &ula_rgba, &frame_buffer_row, &frame_buffer_column)) {
      /* Beam is outside frame buffer. */
      continue;
    }

    tilemap_tick(frame_buffer_row, frame_buffer_column, &tilemap_is_transparent, &tilemap_rgba);
    sprites_tick(frame_buffer_row, frame_buffer_column, &sprites_is_transparent, &sprites_rgba);
    layer2_tick( frame_buffer_row, frame_buffer_column, &layer2_is_transparent,  &layer2_rgba, &layer2_is_priority);

    /* Mix ULA and tilemap. */
    ula_tilemap_is_transparent = ula_is_transparent && tilemap_is_transparent;
    if (!ula_tilemap_is_transparent) {
      ula_tilemap_rgba = (!tilemap_is_transparent && (ula_is_transparent || tilemap_priority_over_ula_get(frame_buffer_row, frame_buffer_column)))
        ? tilemap_rgba
        : ula_rgba;
    }

    /* The default, when no layer specifies a colour. */
    rgba = self.fallback_colour;

    switch (self.layer_priority)
    {
      case E_SLU_LAYER_PRIORITY_SLU:
        if (layer2_is_priority && !layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        } else if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        }
        break;

      case E_SLU_LAYER_PRIORITY_LSU:
        if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        }
        break;

      case E_SLU_LAYER_PRIORITY_LUS:
        if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        }
        break;
        
      case E_SLU_LAYER_PRIORITY_SUL:
        if (layer2_is_priority && !layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        } else if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        }
        break;
 
      case E_SLU_LAYER_PRIORITY_USL:
        if (layer2_is_priority && !layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        } else if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        }
        break;

      case E_SLU_LAYER_PRIORITY_ULS:
        if (layer2_is_priority && !layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!ula_tilemap_is_transparent) {
          rgba = ula_tilemap_rgba;
        } else if (!layer2_is_transparent) {
          rgba = layer2_rgba;
        } else if (!sprites_is_transparent) {
          rgba = sprites_rgba;
        }
        break;

      case E_SLU_LAYER_PRIORITY_BLEND:
      case E_SLU_LAYER_PRIORITY_BLEND_5:
        log_wrn("slu: unimplemented layer priority %d\n", self.layer_priority);
        break;
    }
    
    self.frame_buffer[frame_buffer_row * FRAME_BUFFER_WIDTH + frame_buffer_column] = rgba;
  }

  /* TODO Deal with ULA using 7 MHz clock, not always consuming all ticks. */
  return ticks_14mhz;
}


u32_t slu_active_video_line_get(void) {
  return self.beam_row;
}


void slu_layer_priority_set(slu_layer_priority_t priority) {
  self.layer_priority = priority & 0x07;
}


slu_layer_priority_t slu_layer_priority_get(void) {
  return self.layer_priority;
}


void slu_transparency_fallback_colour_write(u8_t value) {
  self.fallback_colour = PALETTE_UNPACK(value);
}


u8_t slu_line_interrupt_control_read(void) {
  return (ula_irq_asserted()      ? 0x80 : 0x00) |
         (!ula_irq_enable_get()   ? 0x04 : 0x00) |
         (self.line_irq_enabled   ? 0x02 : 0x00) |
         (self.line_irq_row >> 8);
}


void slu_line_interrupt_control_write(u8_t value) {
  ula_irq_enable_set(value & 0x04);

  self.line_irq_enabled = value & 0x02;
  self.line_irq_row     = ((value & 0x01) << 8) | (self.line_irq_row & 0x00FF);
}


u8_t slu_line_interrupt_value_lsb_read(void) {
  return self.line_irq_row & 0x00FF;
}


void slu_line_interrupt_value_lsb_write(u8_t value) {
  self.line_irq_row = (self.line_irq_row & 0x0100) | value;
}
