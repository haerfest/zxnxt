#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "palette.h"
#include "sprites.h"


#define N_SPRITES  128


typedef struct sprite_t {
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

typedef struct sprites_t {
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
  u8_t      pattern_index;
  u16_t     pattern_address;
  u8_t      attribute_index;
  int       is_dirty;
  palette_t palette;
} sprites_t;


static sprites_t sprites;


int sprites_init(void) {
  u8_t number;

  memset(&sprites, 0, sizeof(sprites));

  sprites.patterns       = (u8_t*)     calloc(16, 1024);
  sprites.sprites        = (sprite_t*) calloc(N_SPRITES, sizeof(sprite_t));
  sprites.frame_buffer   = (u16_t*)    malloc(FRAME_BUFFER_HEIGHT * (FRAME_BUFFER_WIDTH / 2) * 2);
  sprites.is_transparent = (u8_t*)     malloc(FRAME_BUFFER_HEIGHT * (FRAME_BUFFER_WIDTH / 2));

  if (sprites.patterns == NULL || sprites.sprites == NULL || sprites.frame_buffer == NULL || sprites.is_transparent == NULL) {
    log_err("sprites: out of memory\n");
    sprites_finit();
    return -1;
  }

  for (number = 0; number < N_SPRITES; number++) {
    sprites.sprites[number].number = number;
  }

  sprites_reset(E_RESET_HARD);

  return 0;
}


void sprites_finit(void) {
  if (sprites.is_transparent != NULL) {
    free(sprites.is_transparent);
    sprites.is_transparent = NULL;
  }
  if (sprites.frame_buffer != NULL) {
    free(sprites.frame_buffer);
    sprites.frame_buffer = NULL;
  }
  if (sprites.sprites != NULL) {
    free(sprites.sprites);
    sprites.sprites = NULL;
  }
  if (sprites.patterns != NULL) {
    free(sprites.patterns);
    sprites.patterns = NULL;
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
  sprites.clip_x1_eff = sprites.clip_x1;
  sprites.clip_x2_eff = sprites.clip_x2;
  sprites.clip_y1_eff = sprites.clip_y1;
  sprites.clip_y2_eff = sprites.clip_y2;

  if (sprites.is_enabled_over_border && sprites.is_enabled_clipping_over_border) {
    sprites.clip_x1_eff *= 2;
    sprites.clip_x2_eff *= 2;
  } else {
    sprites.clip_x1_eff += 32;
    sprites.clip_x2_eff += 32;
    sprites.clip_y1_eff += 32;
    sprites.clip_y2_eff += 32;
  }
}


void sprites_reset(reset_t reset) {
  sprites.is_enabled                      = 0;
  sprites.is_enabled_over_border          = 0;
  sprites.is_enabled_clipping_over_border = 0;
  sprites.is_zero_on_top                  = 0;
  sprites.clip_x1                         = 0;
  sprites.clip_x2                         = 255;
  sprites.clip_y1                         = 0;
  sprites.clip_y2                         = 191;
  sprites.transparency_index              = 0xE3;
  sprites.palette                         = E_PALETTE_SPRITES_FIRST;
  sprites.is_dirty                        = 1;
}


typedef u8_t pattern_t[256];


inline
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


inline
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


inline
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


inline
static void fetch_pattern(int n, int h, pattern_t pattern) {
  if (h) {
    const u8_t* src = &sprites.patterns[n * 128];
    u8_t*       dst = pattern;
    int         i;

    for (i = 0; i < 128; i++) {
      *dst++ = (*src  ) >> 4;
      *dst++ = (*src++) & 0x0F;
    }
  } else {
    memcpy(pattern, &sprites.patterns[(n & 0xFE) * 128], 256);
  }
}


inline
static int is_pixel_visible(int x, int y) {
  if (x < 0 || x >= 320 || y < 0 || y >= 256) {
    /* Outside 320x256 area. */
    return 0;
  }

  if (x < 32 || x >= 320 - 32 || y < 32 || y >= 256 - 32) {
    /* Over border. */
    if (!sprites.is_enabled_over_border) {
      /* Not enabled over border. */
      return 0;
    }
    if (!sprites.is_enabled_clipping_over_border) {
      /* Not clipped over border, only in 256x192 interior. */
      return 1;
    }
  }

  if (x < sprites.clip_x1_eff || x > sprites.clip_x2_eff || y < sprites.clip_y1_eff || y > sprites.clip_y2_eff) {
    /* Clipped. */
    return 0;
  }

  if (sprites.is_zero_on_top && !sprites.is_transparent[y * 320 + x]) {
    /* Earlier higher priority sprite plotted here. */
    return 0;
  }

  return 1;
}


inline
static void draw_pixel(u16_t rgb, int x, int y, int xf, int yf) {
  const int dr = (yf < 0) ? -1 : +1;
  const int dc = (xf < 0) ? -1 : +1;
  int       row;
  int       col;
  int       offset;
  int       xs;
  int       ys;

  for (row = 0; row != yf; row += dr) {
    ys = y + row;
    for (col = 0; col != xf; col += dc) {
      xs = x + col;
      if (is_pixel_visible(xs, ys)) {
        offset = ys * 320 + xs;
        sprites.frame_buffer[  offset] = rgb;
        sprites.is_transparent[offset] = 0;
      }
    }
  }
}


#if 0

static void draw_hex1(u8_t number, u16_t rgb, int x, int y) {
  char patterns[16][5 * 3] = {
    "xxx"
    "x x"
    "x x"
    "x x"
    "xxx",

    "  x"
    "  x"
    "  x"
    "  x"
    "  x",

    "xxx"
    "  x"
    "xxx"
    "x  "
    "xxx",

    "xxx"
    "  x"
    "xxx"
    "  x"
    "xxx",

    "  x"
    "x x"
    "xxx"
    "  x"
    "  x",

    "xxx"
    "x  "
    "xxx"
    "  x"
    "xxx",

    "x  "
    "x  "
    "xxx"
    "x x"
    "xxx",

    "xxx"
    "  x"
    "  x"
    "  x"
    "  x",

    "xxx"
    "x x"
    "xxx"
    "x x"
    "xxx",

    "xxx"
    "x x"
    "xxx"
    "  x"
    "  x",

    "xxx"
    "x x"
    "xxx"
    "x x"
    "x x",

    "xx "
    "x x"
    "xx "
    "x x"
    "xx ",

    "xxx"
    "x  "
    "x  "
    "x  "
    "xxx",

    "xx "
    "x x"
    "x x"
    "x x"
    "xx ",

    "xxx"
    "x  "
    "xxx"
    "x  "
    "xxx",

    "xxx"
    "x  "
    "xxx"
    "x  "
    "x  "
  };

  int   row;
  int   col;
  int   ys;
  int   xs;
  u32_t offset;
  char* ptr = patterns[number];

  for (row = 0; row < 5; row++) {
    ys = y + row;
    for (col = 0; col < 3; col++) {
      xs = x + col;
      if (*ptr++ == 'x') {
        if (is_pixel_visible(xs, ys)) {
          offset = ys * 320 + xs;
          sprites.frame_buffer[  offset] = rgb;
          sprites.is_transparent[offset] = 0;
        }
      }
    }
  }
}

static void draw_hex2(u8_t number, u16_t rgb, int x, int y) {
  draw_hex1(number >> 4  , rgb, x,     y);
  draw_hex1(number & 0x0F, rgb, x + 4, y);
}

#endif


inline
static void draw_pattern(const pattern_t pattern, int p, int x, int y, int xf, int yf, u8_t sprite_number, u8_t pattern_number, u16_t dbg_rgb) {
  const palette_entry_t* entry;
  int                    row;
  int                    col;
  u8_t                   index;

  for (row = 0; row < 16; row++) {
    for (col = 0; col < 16; col++) {
      index = pattern[row * 16 + col];
      if (index != sprites.transparency_index) {
        entry = palette_read_inline(sprites.palette, (p << 4) + index);
        draw_pixel(entry->rgb16, x + col * xf, y + row * yf, xf, yf);
      }
    }
  }

#if 0
  if (is_pixel_visible(x, y)) {
    const u32_t offset = y * 320 + x;
    sprites.frame_buffer[  offset] = dbg_rgb;
    sprites.is_transparent[offset] = 0;

    draw_hex2(sprite_number,  dbg_rgb, x + 2, y + 2);
    draw_hex2(pattern_number, dbg_rgb, x + 2, y + 8);
  }
#endif
}


inline
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

  draw_pattern(pattern, sprite->p, sprite->x80, sprite->y80, 1 << sprite->xx, 1 << sprite->yy, sprite->number, sprite->n60, 0xE000);
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

  draw_pattern(pattern, p, x, y, 1 << sprite->xx, 1 << sprite->yy, sprite->number, n, 0x0E00);
}


inline
static void draw_unified(sprite_t* sprite, const sprite_t* anchor) {
  pattern_t pattern;
  int       x;
  int       y;
  int       n;
  int       p;
  int       xf = 1 << anchor->xx;
  int       yf = 1 << anchor->yy;
  s8_t      xd = (s8_t) sprite->x70;
  s8_t      yd = (s8_t) sprite->y70;
  int       tmp;
  int       xm;
  int       ym;

  n = (sprite->n50 << 1) | sprite->n6;
  if (sprite->po) n += anchor->n60;

  fetch_pattern(n, anchor->h, pattern);

  if (anchor->r) {
    rotate(pattern);
    tmp = xd;
    xd  = -yd;
    yd  = tmp;
  }

  xm = (!anchor->r && anchor->xm) || (anchor->r && anchor->ym);
  ym = (!anchor->r && anchor->ym) || (anchor->r && anchor->xm);

  if (anchor->xm) {
    mirror_x(pattern);
    xd = -xd;
  }

  if (anchor->ym) {
    mirror_y(pattern);
    yd = -yd;
  }

  x = anchor->x80 + xd * xf;
  y = anchor->y80 + yd * yf;
  p = sprite->x8_pr ? (anchor->p + sprite->p) : sprite->p;

  draw_pattern(pattern, p, x, y, xf, yf, sprite->number, n, 0x00E0);
}


inline
static int draw_sprite(sprite_t* sprite, const sprite_t* anchor) {
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


inline
static void draw_sprites(void) {
  const sprite_t* anchor = &initial_anchor;
  sprite_t*       sprite;
  size_t          i;

  /* Assume no sprites visible. */
  memset(sprites.is_transparent, 1, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH / 2);

  /* Draw the sprites in order. */
  for (i = 0; i < N_SPRITES; i++) {
    sprite = &sprites.sprites[i];
    if (draw_sprite(sprite, anchor)) {
      anchor = sprite;
    }
  }
}


inline
static void sprites_tick(u32_t row, u32_t column, int* is_enabled, u16_t* rgb) {
  size_t offset;

  if (!sprites.is_enabled) {
    *is_enabled = 0;
    return;
  }

  if (sprites.is_dirty && row == sprites.clip_y1_eff && column == sprites.clip_x1_eff) {
    draw_sprites();
    sprites.is_dirty = 0;
  }

  offset      = row * FRAME_BUFFER_WIDTH / 2 + column / 2;
  *rgb        = sprites.frame_buffer[offset];
  *is_enabled = !sprites.is_transparent[offset];
}


int sprites_priority_get(void) {
  return sprites.is_zero_on_top;
}


void sprites_priority_set(int is_zero_on_top) {
  if (is_zero_on_top != sprites.is_zero_on_top) {
    sprites.is_zero_on_top = is_zero_on_top;
    sprites.is_dirty       = 1;
  }
}


int sprites_enable_get(void) {
  return sprites.is_enabled;
}


void sprites_enable_set(int enable) {
  if (enable != sprites.is_enabled) {
    sprites.is_enabled = enable;
  }
}


int sprites_enable_over_border_get(void) {
  return sprites.is_enabled_over_border;
}


void sprites_enable_over_border_set(int enable) {
  if (enable != sprites.is_enabled_over_border) {
    sprites.is_enabled_over_border = enable;
    sprites.is_dirty               = 1;

    sprites_update_effective_clipping_area();
  }
}


int sprites_enable_clipping_over_border_get(void) {
  return sprites.is_enabled_clipping_over_border;
}


void sprites_enable_clipping_over_border_set(int enable) {
  if (enable != sprites.is_enabled_clipping_over_border) {
    sprites.is_enabled_clipping_over_border = enable;
    sprites.is_dirty                        = 1;

    sprites_update_effective_clipping_area();
  }
}


void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  if (x1 != sprites.clip_x1 || x2 != sprites.clip_x2 || y1 != sprites.clip_y1 || y2 != sprites.clip_y2) {
    sprites.clip_x1  = x1;
    sprites.clip_x2  = x2;
    sprites.clip_y1  = y1;
    sprites.clip_y2  = y2;
    sprites.is_dirty = 1;

    sprites_update_effective_clipping_area();
  }
}


void sprites_attribute_set(u8_t slot, u8_t attribute_index, u8_t value) {
  sprite_t* sprite = &sprites.sprites[slot & 0x7F];

  if (attribute_index > 4) {
    return;
  }

  sprite->attr[attribute_index] = value;
  sprites.is_dirty                 = 1;

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
  if (value != sprites.transparency_index) {
    sprites.transparency_index = value;
    sprites.is_dirty           = 1;
  }
}


u8_t sprites_slot_get(void) {
  return sprites.sprite_index;
}


void sprites_slot_set(u8_t slot) {
  sprites.sprite_index    = slot & 0x7F;
  sprites.attribute_index = 0;
  sprites.pattern_index   = ((slot & 0x3F) << 1) | ((slot & 0x80) >> 7);
  sprites.pattern_address = sprites.pattern_index * 128;
}


void sprites_next_attribute_set(u8_t value) {
  const sprite_t* sprite = &sprites.sprites[sprites.sprite_index];

  sprites_attribute_set(sprites.sprite_index, sprites.attribute_index, value);

  sprites.attribute_index++;
  if (sprites.attribute_index == 5 || (sprites.attribute_index == 4 && !sprite->e)) {
    sprites.attribute_index = 0;
    sprites.sprite_index    = (sprites.sprite_index + 1) & 0x7F;
  }
}


void sprites_next_pattern_set(u8_t value) {
  if (sprites.patterns[sprites.pattern_address] != value) {
    sprites.patterns[sprites.pattern_address] = value;
    sprites.is_dirty                       = 1;
  }
  sprites.pattern_address = (sprites.pattern_address + 1) & 0x3FFF;
}


void sprites_palette_set(int use_second) {
  const palette_t palette = use_second ? E_PALETTE_SPRITES_SECOND : E_PALETTE_SPRITES_FIRST;
  if (sprites.palette != palette) {
    sprites.palette  = palette;
    sprites.is_dirty = 1;
  }
}
