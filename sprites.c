#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "sprites.h"


#define N_SPRITES  128


typedef struct {
  u8_t          pattern;
  u8_t          n6;
  int           is_pattern_relative;
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
  int           is_unified;
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
  int       is_enabled;
  int       is_enabled_over_border;
  int       is_enabled_clipping_over_border;
  int       is_zero_on_top;
  int       do_clip_in_over_border_mode;
  int       clip_x1;
  int       clip_x2;
  int       clip_y1;
  int       clip_y2;
  u8_t      transparency_index;
  u8_t      slot;
  u8_t      attribute_index;
  sprite_t* last_anchor;
} sprites_t;


static sprites_t self;


int sprites_init(void) {
  memset(&self, 0, sizeof(self));

  self.patterns = (u8_t*) malloc(16 * 1024);
  if (self.patterns == NULL) {
    log_err("sprites: out of memory\n");
    return -1;
  }

  self.sprites = (sprite_t*) malloc(N_SPRITES * sizeof(sprite_t));
  if (self.sprites == NULL) {
    log_err("sprites: out of memory\n");
    free(self.patterns);
    self.patterns = NULL;
    return -1;
  }

  return 0;
}


void sprites_finit(void) {
  if (self.patterns != NULL) {
    free(self.patterns);
    self.patterns = NULL;
  }
  if (self.sprites != NULL) {
    free(self.sprites);
    self.sprites = NULL;
  }
}


void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  *is_transparent = 1;
}


void sprites_priority_set(int is_zero_on_top) {
  self.is_zero_on_top = is_zero_on_top;
}


void sprites_enable_set(int enable) {
  self.is_enabled = enable;
}


void sprites_enable_over_border_set(int enable) {
  self.is_enabled_over_border = enable;
}


void sprites_enable_clipping_over_border_set(int enable) {
  self.is_enabled_clipping_over_border = enable;
}


void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;
}


void sprites_attribute_set(u8_t slot, int attribute_index, u8_t value) {
  sprite_t* sprite = &self.sprites[slot & 0x7F];

  switch (attribute_index) {
    case 0:
      sprite->x = (sprite->x & 0xFF00) | value;
      break;

    case 1:
      sprite->y = (sprite->y & 0xFF00) | value;
      break;

    case 2:
      sprite->palette_offset             = value >> 4;
      sprite->is_mirrored_x              = value & 0x08;
      sprite->is_mirrored_y              = value & 0x04;
      sprite->is_rotated                 = value & 0x02;
      sprite->is_palette_offset_relative = value & 0x01;
      break;

    case 3:
      sprite->is_visible          = value & 0x80;
      sprite->has_fifth_attribute = value & 0x40;
      sprite->pattern             = value & 0x3F;

      if (!sprite->has_fifth_attribute) {
        sprite->is_4bpp    = 0;
        sprite->is_anchor  = 1;
        sprite->is_unified = 0;
      }
      break;

    case 4:
      if (sprite->has_fifth_attribute) {
        sprite->is_anchor = !((value >> 6) == 0x01);
        if (sprite->is_anchor) {
          sprite->is_4bpp             = value & 0x80;
          sprite->is_unified          = value & 0x20;
          sprite->magnification_x     = 1 << ((value & 0x18) >> 3);
          sprite->magnification_y     = 1 << ((value & 0x06) >> 1);
          sprite->is_pattern_relative = value & 0x01;
          if (sprite->is_4bpp) {
            sprite->n6 = (value & 0x40) >> 6;
          }
        } else {
          sprite->n6                  = value >> 5;
          sprite->is_pattern_relative = value & 0x01;
          sprite->magnification_x     = 1 << ((value & 0x18) >> 3);
          sprite->magnification_y     = 1 << ((value & 0x06) >> 1);          
        }
        break;
      }
      /* Fallthrough. */

    default:
      log_wrn("sprites: invalid attribute index %d for sprite %d\n", attribute_index, slot & 0x7F);
      break;
  }
}


void sprites_transparency_index_write(u8_t value) {
  self.transparency_index = value;
}


u8_t sprites_slot_get(void) {
  return self.slot;
}


void sprites_slot_set(u8_t slot) {
  self.slot = slot & 0x7F;
}


void sprites_next_attribute_set(u8_t value) {
  sprites_attribute_set(self.slot, self.attribute_index, value);

  self.attribute_index++;
  if (self.attribute_index == 5 || (self.attribute_index == 4 && !self.sprites[self.slot].has_fifth_attribute)) {
    self.attribute_index = 0;
  }
}


void sprites_next_pattern_set(u8_t value) {
}
