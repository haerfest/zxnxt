#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "sprites.h"


#define N_SPRITES  128


typedef struct {
  u8_t          pattern;
  int           is_pattern_relative;
  int           x;
  int           y;
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


void sprites_attribute_set(u8_t slot, int attribute, u8_t value) {
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
}


void sprites_next_pattern_set(u8_t value) {
}
