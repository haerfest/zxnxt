#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "palette.h"
#include "sprites.h"


#define N_SPRITES  128


typedef struct {
  u8_t number;

  /* Set. */
  u8_t attr[5];

  /* Extracted while set. */
  u8_t x70;
  u8_t y70;
  int  p;
  int  xm;
  int  ym;
  int  r;
  int  x8_pr;
  int  v;
  int  e;
  u8_t n50;
  int  h;
  int  n6;
  int  t;
  int  xx;
  int  yy;
  int  y8;
  int  po;

  /* Calculated for anchors. */
  int  x80;
  int  y80;
  int  n60;
} sprite_t;


const sprite_t initial_anchor = {
  .attr  = {0, 0, 0, 0, 0},
  .x70   = 0,
  .y70   = 0,
  .p     = 0,
  .xm    = 0,
  .ym    = 0,
  .r     = 0,
  .x8_pr = 0,
  .v     = 0,
  .e     = 0,
  .n50   = 0,
  .h     = 0,
  .n6    = 0,
  .t     = 0,
  .xx    = 0,
  .yy    = 0,
  .y8    = 0,
  .po    = 0
};


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


typedef u8_t pattern_t[256];


static void rotate(pattern_t pattern) {
  pattern_t rotated;
  int       row;
  int       col;

  for (row = 0; row < 16; row++) {
    for (col = 0; col < 16; col++) {
      rotated[row * 16 + col] = pattern[(15 - col) * 16 + row];
    }
  }

  memcpy(pattern, rotated, 256);
}


static void mirror_x(pattern_t pattern) {
  int   row;
  int   col;
  u8_t* a;
  u8_t* b;
  u8_t  tmp;

  for (row = 0; row < 16; row++) {
    for (col = 0; col < 8; col++) {
      a = &pattern[row * 16 + col];
      b = &pattern[row * 16 + (15 - col)];

      tmp = *a;
      *a  = *b;
      *b  = tmp;
    }
  }
}


static void mirror_y(pattern_t pattern) {
  int   row;
  int   col;
  u8_t* a;
  u8_t* b;
  u8_t  tmp;

  for (col = 0; col < 16; col++) {
    for (row = 0; row < 8; row++) {
      a = &pattern[row        * 16 + col];
      b = &pattern[(15 - row) * 16 + col];

      tmp = *a;
      *a  = *b;
      *b  = tmp;
    }
  }
}


static void fetch_pattern(int n, int h, pattern_t pattern) {
  if (h) {
    const u8_t* src = &self.patterns[n * 128];
    u8_t*       dst = pattern;
    int         i;

    for (i = 0; i < 128; i++) {
      *dst++ = (*src  ) >> 4;
      *dst++ = (*src++) & 0x0F;
    }
  } else {
    memcpy(pattern, &self.patterns[(n & 0xFE) * 128], 256);
  }
}


static void draw_pixel(u16_t rgb, int x, int y, int xf, int yf) {
  const int dr = (yf < 0) ? -1 : +1;
  const int dc = (xf < 0) ? -1 : +1;
  int       row;
  int       col;
  int       offset;

  for (row = 0; row != yf; row += dr) {
    for (col = 0; col != xf; col += dc) {
      offset = ((y + row + 256) % 256) * 320 + ((x + col + 320) % 320);
      self.frame_buffer[  offset] = rgb;
      self.is_transparent[offset] = 0;
    }
  }
}


static void draw_pattern(const pattern_t pattern, int p, int x, int y, int xf, int yf) {
  const palette_entry_t* entry;
  int                    row;
  int                    col;
  u8_t                   index;

  for (row = 0; row < 16; row++) {
    for (col = 0; col < 16; col++) {
      index = pattern[row * 16 + col];
      if (index != self.transparency_index) {
        entry  = palette_read(self.palette, (p << 4) + index);
        draw_pixel(entry->rgb16, x + col * xf, y + row * yf, xf, yf);
      }
    }
  }
}


static void draw_anchor(sprite_t* sprite) {
  pattern_t pattern;

  if (!sprite->v) {
    return;
  }

  sprite->n60 = (sprite->n50 << 1) | sprite->n6;
  sprite->x80 = (sprite->x8_pr << 8) | sprite->x70;
  sprite->y80 = (sprite->y8    << 8) | sprite->y70;

  fetch_pattern(sprite->n60, sprite->h, pattern);

  if (sprite->r)  rotate(pattern);
  if (sprite->xm) mirror_x(pattern);
  if (sprite->ym) mirror_y(pattern);

  draw_pattern(pattern, sprite->p, sprite->x80, sprite->y80, 1 << sprite->xx, 1 << sprite->yy);
}


static void draw_composite(sprite_t* sprite, const sprite_t* anchor) {
  pattern_t pattern;
  int       x;
  int       y;
  int       n;
  int       p;

  n = (sprite->n50 << 1) | sprite->n6;
  if (sprite->po) n += anchor->n60;

  fetch_pattern(n, anchor->h, pattern);

  if (sprite->r)  rotate(pattern);
  if (sprite->xm) mirror_x(pattern);
  if (sprite->ym) mirror_y(pattern);

  x = anchor->x80 + (s8_t) sprite->x70;
  y = anchor->y80 + (s8_t) sprite->y70;
  p = sprite->x8_pr ? (anchor->p + sprite->p) : sprite->p;

  draw_pattern(pattern, p, x, y, 1 << sprite->xx, 1 << sprite->yy);
}


static void draw_unified(sprite_t* sprite, const sprite_t* anchor) {
}


static int draw_sprite(sprite_t* sprite, const sprite_t* anchor) {

#if 0
  log_wrn("sprites: %03d $%02X %02X %02X %02X %02X v=%d e=%d\n",
          sprite->number,
          sprite->attr[0],
          sprite->attr[1],
          sprite->attr[2],
          sprite->attr[3],
          sprite->attr[4],
          sprite->v,
          sprite->e);
#endif

  if (!sprite->e || (sprite->attr[4] & 0xC0) != 0x40) {
    draw_anchor(sprite);
    return 1;
  }

  if (!anchor->v || !sprite->v) {
    return 0;
  }

  if (anchor->t) {
    draw_unified(sprite, anchor);
  } else {
    draw_composite(sprite, anchor);
  }

  return 0;
}


static void draw_sprites(void) {
  const sprite_t* anchor = &initial_anchor;
  sprite_t*       sprite;
  size_t          i;

  /* Assume no sprites visible. */
  memset(self.is_transparent, 1, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH / 2);

  /* Draw the sprites in order. */
  for (i = 0; i < 128; i++) {
    sprite = &self.sprites[i];
    if (draw_sprite(sprite, anchor)) {
      anchor = sprite;
    }
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

  sprite->attr[attribute_index] = value;
  self.is_dirty                 = 1;

  switch (attribute_index) {
    case 0:
      sprite->x70 = value;
      break;

    case 1:
      sprite->y70 = value;
      break;

    case 2:
      sprite->p     = (value & 0xF0) >> 4;
      sprite->xm    = (value & 0x08) >> 3;
      sprite->ym    = (value & 0x04) >> 2;
      sprite->r     = (value & 0x02) >> 1;
      sprite->x8_pr =  value & 0x01;
      break;

    case 3:
      sprite->v   = value >> 7;
      sprite->e   = (value & 0x40) >> 6;
      sprite->n50 = value & 0x3F;
      if (!sprite->e) {
        sprite->h  = 0;
        sprite->n6 = 0;
        sprite->t  = 0;
        sprite->xx = 0;
        sprite->yy = 0;
        sprite->y8 = 0;
        sprite->po = 0;
      }
      break;

    case 4:
      if (sprite->e) {
        if ((value & 0xC0) != 0x40) {
          sprite->h  = value >> 7;
          sprite->n6 = (value & 0x40) >> 6;
          sprite->t  = (value & 0x20) >> 5;
          sprite->xx = (value & 0x18) >> 3;
          sprite->yy = (value & 0x06) >> 1;
          sprite->y8 =  value & 0x01;
          sprite->po = 0;
        } else {
          sprite->h  = 0;
          sprite->n6 = (value & 0x20) >> 5;
          sprite->t  = 0;
          sprite->xx = (value & 0x18) >> 3;
          sprite->yy = (value & 0x06) >> 1;
          sprite->y8 = 0;
          sprite->po =  value & 0x01;
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
  const sprite_t* sprite = &self.sprites[self.sprite_index];

  sprites_attribute_set(self.sprite_index, self.attribute_index, value);

  self.attribute_index++;
  if (self.attribute_index == 5 || (self.attribute_index == 4 && !sprite->e)) {
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
