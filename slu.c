#include <SDL2/SDL.h>
#include <string.h>
#include "cpu.h"
#include "defs.h"
#include "log.h"
#include "palette.h"
#include "slu.h"


#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define MAX(a,b)  ((a) > (b) ? (a) : (b))


#include "copper.c"
#include "layer2.c"
#include "sprites.c"
#include "tilemap.c"
#include "ula.c"


typedef enum blend_mode_t {
  E_BLEND_MODE_ULA = 0,
  E_BLEND_MODE_NONE,
  E_BLEND_MODE_ULA_TILEMAP_MIX,
  E_BLEND_MODE_TILEMAP
} blend_mode_t;


typedef struct slu_t {
  SDL_Renderer*        renderer;
  SDL_Texture*         texture;
  u16_t*               frame_buffer;
  u32_t                beam_row;
  u32_t                beam_column;
  int                  is_beam_visible;
  u32_t                display_rows;
  u32_t                display_columns;
  u32_t                dirty_row1;
  u32_t                dirty_row2;
  u32_t                dirty_col1;
  u32_t                dirty_col2;

  /* Resettable. */
  slu_layer_priority_t layer_priority;
  int                  line_irq_active;
  int                  line_irq_enabled;
  u16_t                line_irq_row;
  int                  stencil_mode;
  blend_mode_t         blend_mode;
  palette_entry_t      transparent;
  u16_t                fallback_rgba;
} slu_t;


static slu_t self;


int slu_init(SDL_Renderer* renderer, SDL_Texture* texture) {
  memset(&self, 0, sizeof(self));

  self.frame_buffer = malloc(FRAME_BUFFER_SIZE);
  if (self.frame_buffer == NULL) {
    log_err("slu: out of memory\n");
    return -1;
  }

  self.renderer = renderer;
  self.texture  = texture;

  slu_reset(E_RESET_HARD);

  return 0;
}


void slu_finit(void) {
  if (self.frame_buffer != NULL) {
    free(self.frame_buffer);
    self.frame_buffer = NULL;
  }
}


void slu_reset(reset_t reset) {
  self.layer_priority = E_SLU_LAYER_PRIORITY_SLU;
  self.blend_mode     = E_BLEND_MODE_ULA;
  self.stencil_mode   = 0;
  self.fallback_rgba  = palette_rgb8_rgb16(0xE3);

  slu_line_interrupt_enable_set(0);
  slu_transparent_set(0xE3);
}


static void slu_blit(void) {
  if (self.dirty_row1 > self.dirty_row2) {
    return;
  }

  u16_t* pixels;
  int    pitch;

  SDL_Rect src_rect = {
    .y = self.dirty_row1,
    .h = self.dirty_row2 - self.dirty_row1 + 1,
    .x = self.dirty_col1,
    .w = self.dirty_col2 - self.dirty_col1 + 1
  };

  if (SDL_LockTexture(self.texture, &src_rect, (void **) &pixels, &pitch) != 0) {
    log_err("slu: SDL_LockTexture error: %s\n", SDL_GetError());
    return;
  }

  /* Only update the dirty pixels. */
  u16_t* src = &self.frame_buffer[src_rect.y * FRAME_BUFFER_WIDTH + src_rect.x];
  void*  dst = pixels;
  for (int y = 0; y < src_rect.h; y++, dst += pitch, src += FRAME_BUFFER_WIDTH) {
    memcpy(dst, src, src_rect.w * 2);
  }

  SDL_UnlockTexture(self.texture);

  if (SDL_RenderCopy(self.renderer, self.texture, NULL, NULL) != 0) {
    log_err("slu: SDL_RenderCopy error: %s\n", SDL_GetError());
    return;
  }

  SDL_RenderPresent(self.renderer);

  /* Impossible reversed configuration indicates nothing is dirty. */
  self.dirty_row1 = self.dirty_col1 = FRAME_BUFFER_HEIGHT;
  self.dirty_row2 = self.dirty_col2 = 0;
}


/**
 * Beam (0, 0) is the top-left pixel of the (typically) 256x192 content
 * area.
 */
static void slu_beam_advance(void) {
  /* Advance beam one pixel horizontally. */
  if (++self.beam_column < self.display_columns) {
    return;
  }

  /* Advance beam to beginning of next line. */
  self.beam_column = 0;
  if (++self.beam_row < self.display_rows) {
    return;
  }

  /* Move beam to top left of display. */
  self.beam_row = 0;

  /* Update display. */
  slu_blit();

  /* Notify the ULA that we completed a frame. */
  ula_did_complete_frame();  
}


/**
 * https://wiki.specnext.dev/Raster_Interrupt_Control_Register
 *
 * > The line interrupt value uses coordinate system of Copper coprocessor, i.e.
 * > line 0 is the first line of pixels. But the line-interrupt happens already
 * > when the previous line's pixel area is finished (i.e. the raster-line
 * > counter still reads "previous line" and not the one programmed for
 * > interrupt). The INT signal is raised while display beam horizontal position
 * > is between 256-319 standard pixels, precise timing of interrupt handler
 * > execution then depends on how-quickly/if the Z80 will process the INT
 * > signal.
 */
static void slu_irq(void) {
  const int line_irq_active = (self.line_irq_enabled && ((self.beam_row == self.display_rows - 1 && self.line_irq_row == 0) || self.beam_row == self.line_irq_row - 1) && self.beam_column >= 256 * 2);
  if (line_irq_active != self.line_irq_active) {
    self.line_irq_active = line_irq_active;
    cpu_irq(E_CPU_IRQ_LINE, line_irq_active);
  }
}


void slu_mix_layer2(const palette_entry_t* layer2_rgb, const palette_entry_t* mix_rgb, int mix_rgb_transparent, u8_t* mixer_r, u8_t* mixer_g, u8_t* mixer_b) {
  u8_t r = ((layer2_rgb->rgb9 & 0x1C0) >> 2) | ((mix_rgb->rgb9 & 0x1C0) >> 6);
  u8_t g = ((layer2_rgb->rgb9 & 0x038) << 1) | ((mix_rgb->rgb9 & 0x038) >> 3);
  u8_t b = ((layer2_rgb->rgb9 & 0x007) << 4) |  (mix_rgb->rgb9 & 0x007);

  if (self.layer_priority == E_SLU_LAYER_PRIORITY_BLEND) {
    if (r & 0x08) r = 7;
    if (g & 0x08) g = 7;
    if (b & 0x08) b = 7;
  } else if (!mix_rgb_transparent) {
    if (r <= 4) {
      r = 0;
    } else if ((r & 0x0C) == 0x0C) {
      r = 7;
    } else {
      r = r - 5;
    }

    if (g <= 4) {
      g = 0;
    } else if ((g & 0x0C) == 0x0C) {
      g = 7;
    } else {
      g = g - 5;
    }

    if (b <= 4) {
      b = 0;
    } else if ((b & 0x0C) == 0x0C) {
      b = 7;
    } else {
      b = b - 5;
    }
  }

  *mixer_r = r;
  *mixer_g = g;
  *mixer_b = b;
}


void slu_run(u32_t ticks_14mhz) {
  const palette_entry_t black = {
    .rgb8               = 0,
    .rgb9               = 0,
    .rgb16              = 0,
    .is_layer2_priority = 0
  };

  u32_t                  tick;
  u32_t                  frame_buffer_row;
  u32_t                  frame_buffer_column;

  /* These are the same names as in the VHDL for consistency. */
  int                    ula_en;
  int                    ula_border;
  int                    ula_clipped;
  int                    ula_transparent;
  const palette_entry_t* ula_rgb;

  int                    ulatm_transparent;
  const palette_entry_t* ulatm_rgb;

  int                    ula_final_transparent;
  const palette_entry_t* ula_final_rgb;

  int                    ula_mix_transparent;
  const palette_entry_t* ula_mix_rgb;

  int                    tm_en;
  int                    tm_transparent;
  const palette_entry_t* tm_rgb;
  int                    tm_pixel_en;
  int                    tm_pixel_textmode;
  int                    tm_pixel_below;

  int                    sprite_transparent;
  u16_t                  sprite_rgb16;
  int                    sprite_pixel_en;

  int                    layer2_pixel_en;
  int                    layer2_priority;
  int                    layer2_transparent;
  const palette_entry_t* layer2_rgb;

  int                    stencil_transparent;
  palette_entry_t        stencil_rgb;

  int                    mix_rgb_transparent;
  const palette_entry_t* mix_rgb;
  int                    mix_top_transparent;
  const palette_entry_t* mix_top_rgb;
  int                    mix_bot_transparent;
  const palette_entry_t* mix_bot_rgb;

  u8_t                   mixer_r;
  u8_t                   mixer_g;
  u8_t                   mixer_b;

  u16_t                  rgb_out;

  for (tick = 0; tick < ticks_14mhz; tick++) {
    slu_beam_advance();
    slu_irq();

    /* Copper runs at 28 MHz. */
    copper_tick(self.beam_row, self.beam_column, 2);

    if (!ula_beam_to_frame_buffer(self.beam_row, self.beam_column, &frame_buffer_row, &frame_buffer_column)) {
      /* Beam is outside frame buffer. */
      continue;
    }

    ula_tick(    frame_buffer_row, frame_buffer_column, &ula_en, &ula_border, &ula_clipped, &ula_rgb);
    tilemap_tick(frame_buffer_row, frame_buffer_column, &tm_en, &tm_pixel_en, &tm_pixel_below, &tm_pixel_textmode, &tm_rgb);
    sprites_tick(frame_buffer_row, frame_buffer_column, &sprite_pixel_en, &sprite_rgb16);
    layer2_tick( frame_buffer_row, frame_buffer_column, &layer2_pixel_en, &layer2_rgb, &layer2_priority);

    ula_transparent = !ula_en || ula_clipped || (ula_rgb->rgb8 == self.transparent.rgb8);
    tm_transparent  = !tm_en || !tm_pixel_en || (tm_pixel_textmode && tm_rgb->rgb8 == self.transparent.rgb8);

    sprite_transparent = !sprite_pixel_en;

    layer2_transparent = !layer2_pixel_en || (layer2_rgb->rgb8 == self.transparent.rgb8);
    if (layer2_transparent) {
      layer2_priority = 0;
    }

    if (self.stencil_mode && ula_en && tm_en) {
      stencil_transparent   = ula_transparent || tm_transparent;
      stencil_rgb.rgb16     = !stencil_transparent ? (ula_rgb->rgb16 & tm_rgb->rgb16) : 0;
      ula_final_rgb         = &stencil_rgb;
      ula_final_transparent = stencil_transparent;
    } else {
      if (ula_transparent) ula_rgb = &black;
      if (tm_transparent)  tm_rgb  = &black;

      ulatm_transparent     = ula_transparent && tm_transparent;
      ulatm_rgb             = (!tm_transparent && (!tm_pixel_below || ula_transparent)) ? tm_rgb : ula_rgb;
      ula_final_rgb         = ulatm_rgb;
      ula_final_transparent = ulatm_transparent;
    }

    rgb_out = self.fallback_rgba;

    switch (self.layer_priority)
    {
      case E_SLU_LAYER_PRIORITY_SLU:
        if (layer2_priority) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        } else if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!ula_final_transparent) {
          rgb_out = ula_final_rgb->rgb16;
        }
        break;

      case E_SLU_LAYER_PRIORITY_LSU:
        if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        } else if (!ula_final_transparent) {
          rgb_out = ula_final_rgb->rgb16;
        }
        break;

      case E_SLU_LAYER_PRIORITY_SUL:
        if (layer2_priority) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        } else if (!ula_final_transparent) {
          rgb_out = ula_final_rgb->rgb16;
        } else if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        }
        break;

      case E_SLU_LAYER_PRIORITY_LUS:
        if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!ula_final_transparent && !(ula_border && tm_transparent && !sprite_transparent)) {
          rgb_out = ula_final_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        }
        break;
        
      case E_SLU_LAYER_PRIORITY_USL:
        if (layer2_priority) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!ula_final_transparent && !(ula_border && tm_transparent && !sprite_transparent)) {
          rgb_out = ula_final_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        } else if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        }
        break;

      case E_SLU_LAYER_PRIORITY_ULS:
        if (layer2_priority) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!ula_final_transparent && !(ula_border && tm_transparent && !sprite_transparent)) {
          rgb_out = ula_final_rgb->rgb16;
        } else if (!layer2_transparent) {
          rgb_out = layer2_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        }
        break;

      case E_SLU_LAYER_PRIORITY_BLEND:
      case E_SLU_LAYER_PRIORITY_BLEND_5:
        ula_mix_transparent = ula_clipped || (ula_rgb->rgb8 == self.transparent.rgb8);
        ula_mix_rgb         = ula_mix_transparent ? ula_rgb : &black;

        switch (self.blend_mode) {
          case E_BLEND_MODE_ULA:
            mix_rgb             = ula_mix_rgb;
            mix_rgb_transparent = ula_mix_transparent;
            mix_top_transparent = tm_transparent || tm_pixel_below;
            mix_top_rgb         = tm_rgb;
            mix_bot_transparent = tm_transparent || !tm_pixel_below;
            mix_bot_rgb         = tm_rgb;
            break;

          case E_BLEND_MODE_ULA_TILEMAP_MIX:
            mix_rgb             = ula_final_rgb;
            mix_rgb_transparent = ula_final_transparent;
            mix_top_transparent = 1;
            mix_top_rgb         = tm_rgb;
            mix_bot_transparent = 1;
            mix_bot_rgb         = tm_rgb;
            break;

          case E_BLEND_MODE_TILEMAP:
            mix_rgb             = tm_rgb;
            mix_rgb_transparent = tm_transparent;
            mix_top_transparent = ula_transparent || !tm_pixel_below;
            mix_top_rgb         = ula_rgb;
            mix_bot_transparent = ula_transparent || tm_pixel_below;
            mix_bot_rgb         = ula_rgb;
            break;

          default:
            mix_rgb             = 0;
            mix_rgb_transparent = 1;
            if (tm_pixel_below) {
              mix_top_transparent = ula_transparent;
              mix_top_rgb         = ula_rgb;
              mix_bot_transparent = tm_transparent;
              mix_bot_rgb         = tm_rgb;
            } else {
              mix_top_transparent = tm_transparent;
              mix_top_rgb         = tm_rgb;
              mix_bot_transparent = ula_transparent;
              mix_bot_rgb         = ula_rgb;
            }
            break;
        }

        if (layer2_priority) {
          slu_mix_layer2(layer2_rgb, mix_rgb, mix_rgb_transparent, &mixer_r, &mixer_g, &mixer_b);
          rgb_out = (mixer_r << 13) | (mixer_g << 9) | (mixer_b << 5);
        } else if (!mix_top_transparent) {
          rgb_out = mix_top_rgb->rgb16;
        } else if (!sprite_transparent) {
          rgb_out = sprite_rgb16;
        } else if (!mix_bot_transparent) {
          rgb_out = mix_bot_rgb->rgb16;
        } else if (!layer2_transparent) {
          slu_mix_layer2(layer2_rgb, mix_rgb, mix_rgb_transparent, &mixer_r, &mixer_g, &mixer_b);
          rgb_out = (mixer_r << 13) | (mixer_g << 9) | (mixer_b << 5);
        }
        break;
    }
    
    u16_t* rgb_existing = &self.frame_buffer[frame_buffer_row * FRAME_BUFFER_WIDTH + frame_buffer_column];
    if (rgb_out != *rgb_existing) {
      *rgb_existing = rgb_out;

      self.dirty_row1 = MIN(self.dirty_row1, frame_buffer_row);
      self.dirty_row2 = MAX(self.dirty_row2, frame_buffer_row);
      self.dirty_col1 = MIN(self.dirty_col1, frame_buffer_column);
      self.dirty_col2 = MAX(self.dirty_col2, frame_buffer_column);
    }
  }
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
  self.fallback_rgba = palette_rgb8_rgb16(value);
}


u8_t slu_line_interrupt_control_read(void) {
  return (self.line_irq_active  ? 0x80 : 0x00) |
         (!ula_irq_enable_get() ? 0x04 : 0x00) |
         (self.line_irq_enabled ? 0x02 : 0x00) |
         (self.line_irq_row >> 8);
}


void slu_line_interrupt_control_write(u8_t value) {
  ula_irq_enable_set(!(value & 0x04));

  self.line_irq_row = ((value & 0x01) << 8) | (self.line_irq_row & 0x00FF);
  slu_line_interrupt_enable_set(value & 0x02);
}


u8_t slu_line_interrupt_value_lsb_read(void) {
  return self.line_irq_row & 0x00FF;
}


void slu_line_interrupt_value_lsb_write(u8_t value) {
  self.line_irq_row = (self.line_irq_row & 0x0100) | value;
}


void slu_ula_control_write(u8_t value) {
  ula_enable_set((value & 0x80) == 0);
  self.blend_mode               = (value & 0x60) >> 5;
  self.stencil_mode             = value & 0x01;
}


void slu_transparent_set(u8_t rgb8) {
  self.transparent.rgb8               = rgb8;
  self.transparent.rgb9               = PALETTE_RGB8_TO_RGB9(rgb8);
  self.transparent.rgb16              = PALETTE_RGB9_TO_RGB16(self.transparent.rgb9);
  self.transparent.is_layer2_priority = 0;
}


const palette_entry_t* slu_transparent_get(void) {
  return &self.transparent;
}


void slu_line_interrupt_enable_set(int enable) {
  self.line_irq_enabled = enable;
  if (!self.line_irq_enabled) {
    self.line_irq_active = 0;
    cpu_irq(E_CPU_IRQ_LINE, 0);
  }  
}


void slu_display_size_set(unsigned int rows, unsigned int columns) {
  self.display_rows    = rows;
  self.display_columns = columns;
}
