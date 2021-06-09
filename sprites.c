#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "palette.h"
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
  u16_t*    frame_buffer;
  u8_t*     is_transparent;
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
  u16_t     transparency_rgba;
  u8_t      sprite_index;
  u16_t     pattern_index;
  u8_t      attribute_index;
} sprites_t;


static sprites_t self;


int sprites_init(void) {
  memset(&self, 0, sizeof(self));

  self.patterns       = (u8_t*)     calloc(16, 1024);
  self.sprites        = (sprite_t*) calloc(N_SPRITES, sizeof(sprite_t));
  self.frame_buffer   = (u16_t*)    malloc(FRAME_BUFFER_WIDTH / 2 * FRAME_BUFFER_HEIGHT * 2);
  self.is_transparent = (u8_t*)     malloc(FRAME_BUFFER_WIDTH / 2 * FRAME_BUFFER_HEIGHT);

  if (self.patterns == NULL || self.sprites == NULL || self.frame_buffer == NULL || self.is_transparent == NULL) {
    log_err("sprites: out of memory\n");
    sprites_finit();
    return -1;
  }

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


void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  const size_t offset = row * FRAME_BUFFER_WIDTH / 2 + column / 2;

  if (!self.is_enabled) {
    *is_transparent = 1;
    return;
  }

  if (self.is_transparent[offset]) {
    *is_transparent = 1;
    return;
  }

  *rgba           = self.frame_buffer[offset];
  *is_transparent = *rgba == self.transparency_rgba;
}


static void draw(const sprite_t* sprite, const sprite_t* anchor) {
  u8_t* pattern;
  u16_t row;
  u16_t column;
  u8_t  index;
  palette_entry_t entry;
  size_t offset;
  u16_t  sprite_x;

  if (!sprite->is_anchor) {
    return;
  }
  if (sprite->is_4bpp) {
    return;
  }
  if (sprite->is_visible) {
    return;
  }

  sprite_x = (sprite->is_palette_offset_relative << 8) | (sprite->x & 0x00FF);

  pattern = &self.patterns[sprite->pattern * 256];

  for (row = 0; row < 16; row++) {
    for (column = 0; column < 16; column++) {
      index = *pattern++;
      if (index != self.transparency_index) {
        entry  = palette_read(E_PALETTE_SPRITES_FIRST, index);
        offset = (sprite->y + row) * FRAME_BUFFER_WIDTH / 2 + sprite_x + column;
        if (entry.rgba != self.transparency_rgba) {
          self.frame_buffer[offset]   = entry.rgba;
          self.is_transparent[offset] = 1;
        }
      }
    }
  }
}


static void update(void) {
  sprite_t* sprite;
  sprite_t* anchor;
  size_t    i;

  if (!self.is_enabled) {
    return;
  }

  /* Assume no sprites visible. */
  memset(self.is_transparent, 1, sizeof(FRAME_BUFFER_WIDTH / 2 * FRAME_BUFFER_HEIGHT));

  /* Draw the sprites in order. */
  anchor = NULL;
  for (i = 0; i < 128; i++) {
    sprite = &self.sprites[i];
    if (sprite->is_anchor) {
      anchor = sprite;
    }
    draw(sprite, anchor);
  }
}


void sprites_priority_set(int is_zero_on_top) {
  self.is_zero_on_top = is_zero_on_top;
}


void sprites_enable_set(int enable) {
  if (enable != self.is_enabled) {
    self.is_enabled = enable;
    update();
  }
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
      update();
      break;

    case 1:
      sprite->y = (sprite->y & 0xFF00) | value;
      update();
      break;

    case 2:
      sprite->palette_offset             = value >> 4;
      sprite->is_mirrored_x              = value & 0x08;
      sprite->is_mirrored_y              = value & 0x04;
      sprite->is_rotated                 = value & 0x02;
      sprite->is_palette_offset_relative = value & 0x01;
      update();            
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
      update();
      break;

    case 4:
      if (sprite->has_fifth_attribute) {
        sprite->is_anchor = !((value >> 6) == 0x01);
        if (sprite->is_anchor) {
          sprite->is_4bpp             = value & 0x80;
          if (sprite->is_4bpp) {
            sprite->n6 = (value & 0x40) >> 6;
          }
          sprite->is_unified          = value & 0x20;
          sprite->magnification_x     = 1 << ((value & 0x18) >> 3);
          sprite->magnification_y     = 1 << ((value & 0x06) >> 1);
          sprite->y                   = ((value & 0x01) << 8) | (sprite->y & 0x00FF);
        } else {
          sprite->n6                  = value >> 5;
          sprite->magnification_x     = 1 << ((value & 0x18) >> 3);
          sprite->magnification_y     = 1 << ((value & 0x06) >> 1);
          sprite->is_pattern_relative = value & 0x01;
        }
        update();
        break;
      }
      /* Fallthrough. */

    default:
      log_wrn("sprites: invalid attribute index %d for sprite %d\n", attribute_index, slot & 0x7F);
      break;
  }
}


/* TODO: What if the colour behind this index changes behind our back? */
void sprites_transparency_index_write(u8_t value) {
  if (value != self.transparency_index) {
    self.transparency_index = value;
    update();
  }
}


/* TODO: Not sure if sprites use the global transparency colour. */
void sprites_transparency_colour_write(u8_t rgb) {
  self.transparency_rgba = PALETTE_UNPACK(rgb);
}


u8_t sprites_slot_get(void) {
  return self.sprite_index;
}


void sprites_slot_set(u8_t slot) {
  self.sprite_index  = slot & 0x7F;
  self.pattern_index = (((slot & 0x3F) << 1) | (slot >> 7)) * 256;
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
  self.patterns[self.pattern_index] = value;
  self.pattern_index = (self.pattern_index + 1) & 0x3FFF;
  update();
}
