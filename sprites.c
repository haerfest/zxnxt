#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "palette.h"
#include "sprites.h"


#define N_SPRITES  128


typedef struct {
  u8_t          number;
  u8_t          attribute[5];
  u8_t          pattern_index;
  u8_t          n6;
  int           is_pattern_index_relative;
  u16_t         x;
  u16_t         y;
  int           palette_offset;
  int           is_palette_offset_relative;
  int           is_rotated;
  int           is_mirrored_x;
  int           is_mirrored_y;
  int           is_visible;
  int           has_fifth_attribute;
  int           is_4bpp;
  int           is_anchor;
  int           are_relative_unified;
  int           magnification_x;
  int           magnification_y;
} sprite_t;


/**
 * https://wiki.specnext.dev/Sprites
 *
 * "The images are stored in a 16K memory internal to the FPGA"
 *
 * That's 64 x 8-bit sprites of 256 bytes each, or
 *       128 x 4-bit sprites of 128 bytes each.
 */

typedef struct {
  u8_t*     patterns;
  sprite_t* sprites;
  u16_t*    frame_buffer;
  u8_t*     is_transparent;
  int       is_enabled;
  int       is_enabled_over_border;
  int       is_enabled_clipping_over_border;
  int       is_zero_on_top;
  int       clip_x1;
  int       clip_x2;
  int       clip_y1;
  int       clip_y2;
  int       clip_x1_eff;
  int       clip_x2_eff;
  int       clip_y1_eff;
  int       clip_y2_eff;  
  u8_t      transparency_index;
  u8_t      sprite_index;
  u16_t     pattern_index;
  u8_t      attribute_index;
  int       is_dirty;
  palette_t palette;
} sprites_t;


static sprites_t self;


int sprites_init(void) {
  u8_t number;

  memset(&self, 0, sizeof(self));

  self.patterns       = (u8_t*)     calloc(16, 1024);
  self.sprites        = (sprite_t*) calloc(N_SPRITES, sizeof(sprite_t));
  self.frame_buffer   = (u16_t*)    malloc(FRAME_BUFFER_HEIGHT * (FRAME_BUFFER_WIDTH / 2) * 2);
  self.is_transparent = (u8_t*)     malloc(FRAME_BUFFER_HEIGHT * (FRAME_BUFFER_WIDTH / 2));

  if (self.patterns == NULL || self.sprites == NULL || self.frame_buffer == NULL || self.is_transparent == NULL) {
    log_err("sprites: out of memory\n");
    sprites_finit();
    return -1;
  }

  for (number = 0; number < N_SPRITES; number++) {
    self.sprites[number].number = number;
  }

  sprites_reset(E_RESET_HARD);

  return 0;
}


void sprites_finit(void) {
  if (self.is_transparent != NULL) {
    free(self.is_transparent);
    self.is_transparent = NULL;
  }
  if (self.frame_buffer != NULL) {
    free(self.frame_buffer);
    self.frame_buffer = NULL;
  }
  if (self.sprites != NULL) {
    free(self.sprites);
    self.sprites = NULL;
  }
  if (self.patterns != NULL) {
    free(self.patterns);
    self.patterns = NULL;
  }
}


static void sprites_update_effective_clipping_area(void) {
  /**
   * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt
   *
   * > When the clip window is enabled for sprites in "over border" mode,
   * > the X coords are internally doubled and the clip window origin is
   * > moved to the sprite origin inside the border.
   */
  self.clip_x1_eff = self.clip_x1;
  self.clip_x2_eff = self.clip_x2;
  self.clip_y1_eff = self.clip_y1;
  self.clip_y2_eff = self.clip_y2;

  if (self.is_enabled_over_border) {
    self.clip_x1_eff *= 2;
    self.clip_x2_eff *= 2;
  } else {
    self.clip_x1_eff += 32;
    self.clip_x2_eff += 32;
    self.clip_y1_eff += 32;
    self.clip_y2_eff += 32;
  }
}


void sprites_reset(reset_t reset) {
  self.is_enabled                      = 0;
  self.is_enabled_over_border          = 0;
  self.is_enabled_clipping_over_border = 0;
  self.is_zero_on_top                  = 0;
  self.clip_x1                         = 0;
  self.clip_x2                         = 255;
  self.clip_y1                         = 0;
  self.clip_y2                         = 191;
  self.transparency_index              = 0xE3;
  self.palette                         = E_PALETTE_SPRITES_FIRST;
  self.is_dirty                        = 1;
}


static void draw_sprite(const sprite_t* sprite, const sprite_t* anchor) {
  u8_t*                  pattern;
  u8_t                   sprite_row;
  u8_t                   sprite_column;
  u8_t                   projected_row;
  u8_t                   projected_column;
  u8_t                   index;
  const palette_entry_t* entry;
  size_t                 offset;
  s16_t                  sprite_x;
  s16_t                  sprite_y;
  u8_t                   palette_offset;
  u8_t                   pattern_index;
  u16_t                  final_x;
  u16_t                  final_y;
  int                    is_4bpp;
  u8_t                   magnified_row;
  u8_t                   magnified_column;
  u8_t                   magnification_x;
  u8_t                   magnification_y;
  int                    is_rotated;
  int                    is_mirrored_x;
  int                    is_mirrored_y;

  if (!sprite->is_visible) {
    return;
  }

  if (!sprite->is_anchor && anchor && !anchor->is_visible) {
    return;
  }

  if (!sprite->is_anchor && anchor) {
    sprite_x       = ((anchor->is_palette_offset_relative << 8) | (anchor->x & 0x00FF)) + (s8_t) sprite->x;
    sprite_y       = anchor->y + (s8_t) sprite->y;
    palette_offset = sprite->is_palette_offset_relative ? (anchor->palette_offset + sprite->palette_offset) : sprite->palette_offset;
    pattern_index  = sprite->is_pattern_index_relative  ? (anchor->pattern_index  + sprite->pattern_index ) : sprite->pattern_index;
    is_4bpp        = anchor->is_4bpp;
    if (anchor->are_relative_unified) {
      is_rotated      = anchor->is_rotated;
      is_mirrored_x   = anchor->is_mirrored_x;
      is_mirrored_y   = anchor->is_mirrored_y;
      magnification_x = 1 << anchor->magnification_x;
      magnification_y = 1 << anchor->magnification_y;
    } else {
      is_rotated      = sprite->is_rotated;
      is_mirrored_x   = sprite->is_mirrored_x;
      is_mirrored_y   = sprite->is_mirrored_y;
      magnification_x = 1 << sprite->magnification_x;
      magnification_y = 1 << sprite->magnification_y;
    }
  } else {
    sprite_x        = (sprite->is_palette_offset_relative << 8) | (sprite->x & 0x00FF);
    sprite_y        = sprite->y;
    palette_offset  = sprite->palette_offset;
    pattern_index   = sprite->pattern_index;
    is_4bpp         = sprite->is_4bpp;
    is_rotated      = sprite->is_rotated;
    is_mirrored_x   = sprite->is_mirrored_x;
    is_mirrored_y   = sprite->is_mirrored_y;
    magnification_x = 1 << sprite->magnification_x;
    magnification_y = 1 << sprite->magnification_y;
  }

#if 0
  log_wrn("sprites: sprite #%03d %02X %02X %02X %02X %02X type=%s pattern_index=%03d\n",
          sprite->number,
          sprite->attribute[0],
          sprite->attribute[1],
          sprite->attribute[2],
          sprite->attribute[3],
          sprite->attribute[4],
          (sprite->is_anchor ? "anchor" : (!anchor ? "?" : (anchor->are_relative_unified ? "unified" : "composite"))),
          pattern_index);
#endif

  if (is_4bpp) {
    pattern_index = (pattern_index << 1) | sprite->n6;
    pattern       = &self.patterns[pattern_index * 128];
  } else {
    pattern       = &self.patterns[pattern_index * 256];
  }

  for (sprite_row = 0; sprite_row < 16; sprite_row++) {
    for (sprite_column = 0; sprite_column < 16; sprite_column++) {

      /* Read the colour value from the pattern. */
      if (is_4bpp) {
        if (sprite_row & 1) {
          index = (*pattern++) & 0x0F;
        } else {
          index = (*pattern) >> 4;
        }
      } else {
        index = *pattern++;
      }

      if (index == self.transparency_index) {
        continue;
      }

      entry = palette_read(self.palette, palette_offset + index);

      /* Then figure out where to project it to. */
      for (magnified_row = sprite_row * magnification_y; magnified_row < (sprite_row + 1) * magnification_y; magnified_row++) {
        for (magnified_column = sprite_column * magnification_x; magnified_column < (sprite_column + 1) * magnification_x; magnified_column++) {

          /* Apply rotation first. */
          projected_column = is_rotated ? magnified_row    : magnified_column;
          projected_row    = is_rotated ? magnified_column : magnified_row;

          /* Then mirroring. */
          projected_column = is_mirrored_x ? (16 * magnification_x - 1 - projected_column) : projected_column;
          projected_row    = is_mirrored_y ? (16 * magnification_y - 1 - projected_row)    : projected_row;

          final_x = sprite_x + projected_column;
          final_y = sprite_y + projected_row;
          if (final_x >= FRAME_BUFFER_WIDTH / 2) {
            continue;
          }
          if (final_y >= FRAME_BUFFER_HEIGHT) {
            continue;
          }

          if (final_x < 32 || final_x >= 32 + 256 || final_y < 32 || final_y >= 32 + 192) {
            if (!self.is_enabled_over_border) {
              continue;
            }
            if (self.is_enabled_clipping_over_border) {
              if (final_x < self.clip_x1_eff || final_x > self.clip_x2_eff || final_y < self.clip_y1_eff || final_y > self.clip_y2_eff) {
                continue;
              }
            }
          } else if (final_x < self.clip_x1_eff || final_x > self.clip_x2_eff || final_y < self.clip_y1_eff || final_y > self.clip_y2_eff) {
            continue;
          }

          offset = final_y * FRAME_BUFFER_WIDTH / 2 + final_x;

          self.frame_buffer[offset]   = entry->rgb16;
          self.is_transparent[offset] = 0;
        }
      }
    }
  }
}


static void draw_sprites(void) {
  sprite_t* sprite;
  sprite_t* anchor;
  size_t    i;

  /* Assume no sprites visible. */
  memset(self.is_transparent, 1, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH / 2);

  /* Draw the sprites in order. */
  anchor = NULL;
  for (i = 0; i < 128; i++) {
    sprite = &self.sprites[i];
    if (sprite->is_anchor) {
      anchor = sprite;
    }
    draw_sprite(sprite, anchor);
  }
}


void sprites_tick(u32_t row, u32_t column, int* is_enabled, u16_t* rgb) {
  size_t offset;

  if (!self.is_enabled) {
    *is_enabled = 0;
    return;
  }

  if (self.is_dirty && row == self.clip_y1_eff && column == self.clip_x1_eff) {
    draw_sprites();
    self.is_dirty = 0;
  }

  offset      = row * FRAME_BUFFER_WIDTH / 2 + column / 2;
  *rgb        = self.frame_buffer[offset];
  *is_enabled = !self.is_transparent[offset];
}


int sprites_priority_get(void) {
  return self.is_zero_on_top;
}


void sprites_priority_set(int is_zero_on_top) {
  if (is_zero_on_top != self.is_zero_on_top) {
    self.is_zero_on_top = is_zero_on_top;
    self.is_dirty       = 1;
  }
}


int sprites_enable_get(void) {
  return self.is_enabled;
}


void sprites_enable_set(int enable) {
  if (enable != self.is_enabled) {
    self.is_enabled = enable;
  }
}


int sprites_enable_over_border_get(void) {
  return self.is_enabled_over_border;
}


void sprites_enable_over_border_set(int enable) {
  if (enable != self.is_enabled_over_border) {
    self.is_enabled_over_border = enable;
    self.is_dirty               = 1;

    sprites_update_effective_clipping_area();
  }
}


int sprites_enable_clipping_over_border_get(void) {
  return self.is_enabled_clipping_over_border;
}


void sprites_enable_clipping_over_border_set(int enable) {
  if (enable != self.is_enabled_clipping_over_border) {
    self.is_enabled_clipping_over_border = enable;
    self.is_dirty                        = 1;

    sprites_update_effective_clipping_area();
  }
}


void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  if (x1 != self.clip_x1 || x2 != self.clip_x2 || y1 != self.clip_y1 || y2 != self.clip_y2) {
    self.clip_x1  = x1;
    self.clip_x2  = x2;
    self.clip_y1  = y1;
    self.clip_y2  = y2;
    self.is_dirty = 1;

    sprites_update_effective_clipping_area();
  }
}


void sprites_attribute_set(u8_t slot, u8_t attribute_index, u8_t value) {
  sprite_t* sprite = &self.sprites[slot & 0x7F];

  if (attribute_index > 4) {
    return;
  }

  sprite->attribute[attribute_index] = value;
  self.is_dirty                      = 1;

  switch (attribute_index) {
    case 0:
      sprite->x = (sprite->x & 0xFF00) | value;
      break;

    case 1:
      sprite->y = (sprite->y & 0xFF00) | value;
      break;

    case 2:
      sprite->palette_offset             = value & 0xF0;
      sprite->is_mirrored_x              = value & 0x08;
      sprite->is_mirrored_y              = value & 0x04;
      sprite->is_rotated                 = value & 0x02;
      sprite->is_palette_offset_relative = value & 0x01;
      break;

    case 3:
      sprite->is_visible          = value & 0x80;
      sprite->has_fifth_attribute = value & 0x40;
      sprite->pattern_index       = value & 0x3F;

      if (!sprite->has_fifth_attribute) {
        sprite->is_4bpp              = 0;
        sprite->is_anchor            = 1;
        sprite->are_relative_unified = 0;
      }
      break;

    case 4:
      if (sprite->has_fifth_attribute) {
        sprite->is_anchor = ((value >> 6) != 0x01);
        if (sprite->is_anchor) {
          sprite->is_4bpp = value & 0x80;
          if (sprite->is_4bpp) {
            sprite->n6 = (value & 0x40) >> 6;
          }
          sprite->are_relative_unified = value & 0x20;
          sprite->magnification_x      = (value & 0x18) >> 3;
          sprite->magnification_y      = (value & 0x06) >> 1;
          sprite->y                    = ((value & 0x01) << 8) | (sprite->y & 0x00FF);
        } else {
          sprite->n6                        = (value & 0x20) >> 5;
          sprite->magnification_x           = (value & 0x18) >> 3;
          sprite->magnification_y           = (value & 0x06) >> 1;
          sprite->is_pattern_index_relative = value & 0x01;
        }
      }
      break;
  }
}


void sprites_transparency_index_write(u8_t value) {
  if (value != self.transparency_index) {
    self.transparency_index = value;
    self.is_dirty           = 1;
  }
}


u8_t sprites_slot_get(void) {
  return self.sprite_index;
}


void sprites_slot_set(u8_t slot) {
  self.sprite_index  = slot & 0x7F;
  self.pattern_index = (slot & 0x3F) * 256 + (slot & 0x80);
}


void sprites_next_attribute_set(u8_t value) {
  sprites_attribute_set(self.sprite_index, self.attribute_index, value);

  self.attribute_index++;
  if (self.attribute_index == 5 || (self.attribute_index == 4 && !self.sprites[self.sprite_index].has_fifth_attribute)) {
    self.attribute_index = 0;
    self.sprite_index    = (self.sprite_index + 1) & 0x7F;
  }
}


void sprites_next_pattern_set(u8_t value) {
  if (self.patterns[self.pattern_index] != value) {
    self.patterns[self.pattern_index] = value;
    self.is_dirty                     = 1;
  }
  self.pattern_index = (self.pattern_index + 1) & 0x3FFF;
}


void sprites_palette_set(int use_second) {
  const palette_t palette = use_second ? E_PALETTE_SPRITES_SECOND : E_PALETTE_SPRITES_FIRST;
  if (self.palette != palette) {
    self.palette  = palette;
    self.is_dirty = 1;
  }
}
