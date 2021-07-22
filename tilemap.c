#include <string.h>
#include "log.h"
#include "memory.h"
#include "palette.h"
#include "tilemap.h"


/**
 * TODO:
 * - tile rotation
 * - tile mirroring
 */


typedef struct tilemap_t {
  u8_t*     bank5;
  u8_t      default_attribute;
  u16_t     definitions_base_address;
  u16_t     tilemap_base_address;
  u8_t      transparency_index;
  int       is_enabled;
  int       use_80x32;
  int       use_default_attribute;
  int       use_text_mode;
  int       use_512_tiles;
  int       tilemap_over_ula;
  palette_t palette;
  u8_t      offset_y;
  u16_t     offset_x;
  int       clip_x1;
  int       clip_x2;
  int       clip_y1;
  int       clip_y2;
} tilemap_t;


static tilemap_t tilemap;


int tilemap_init(u8_t* sram) {
  memset(&tilemap, 0, sizeof(tilemap));

  tilemap.bank5 = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];

  tilemap_reset(E_RESET_HARD);

  return 0;
}


void tilemap_finit(void) {
}


void tilemap_reset(reset_t reset) {
  tilemap.definitions_base_address = 0x0C00;
  tilemap.tilemap_base_address     = 0x2C00;
  tilemap.transparency_index       = 0x0F;
  tilemap.clip_x1                  = 0;
  tilemap.clip_x2                  = 159;
  tilemap.clip_y1                  = 0;
  tilemap.clip_y2                  = 255;
  tilemap.offset_x                 = 0;
  tilemap.offset_y                 = 0;
  tilemap.is_enabled               = 0;
  tilemap.use_80x32                = 0;
  tilemap.use_default_attribute    = 0;
  tilemap.palette                  = E_PALETTE_TILEMAP_FIRST;
  tilemap.use_text_mode            = 0;
  tilemap.use_512_tiles            = 0;
  tilemap.tilemap_over_ula         = 0;
  tilemap.default_attribute        = 0x00;
}


void tilemap_tilemap_control_write(u8_t value) {
  tilemap.is_enabled            = value & 0x80;
  tilemap.use_80x32             = value & 0x40;
  tilemap.use_default_attribute = value & 0x20;
  tilemap.use_text_mode         = value & 0x08;
  tilemap.use_512_tiles         = value & 0x02;
  tilemap.tilemap_over_ula      = value & 0x01;

  tilemap.palette = (value & 0x10) ? E_PALETTE_TILEMAP_SECOND : E_PALETTE_TILEMAP_FIRST;
}


void tilemap_default_tilemap_attribute_write(u8_t value) {
  tilemap.default_attribute = value;
}


void tilemap_tilemap_base_address_write(u8_t value) {
  tilemap.tilemap_base_address = (value & 0x3F) << 8;
}


void tilemap_tilemap_tile_definitions_address_write(u8_t value) {
  tilemap.definitions_base_address = (value & 0x3F) << 8;
}


void tilemap_transparency_index_write(u8_t value) {
  tilemap.transparency_index = value;
}


inline
static u16_t tilemap_map_offset_get(u32_t row, u32_t column) {
  const u8_t  map_row    = row    / 8;
  const u8_t  map_column = column / (tilemap.use_80x32 ? 8 : 16);

  return tilemap.tilemap_base_address + (map_row * (tilemap.use_80x32 ? 80 : 40) + map_column) * (tilemap.use_default_attribute ? 1 : 2);
}


inline
static u8_t tilemap_attribute_get(u32_t row, u32_t column) {
  u16_t map_offset;

  if (tilemap.use_default_attribute) {
    return tilemap.default_attribute;
  }
  
  map_offset = tilemap_map_offset_get(row, column);
  return tilemap.bank5[map_offset + 1];
}


inline
static void tilemap_tick(u32_t row, u32_t column, int* is_enabled, int* is_pixel_enabled, int* is_pixel_below, int* is_pixel_textmode, const palette_entry_t** rgb) {
  if (!tilemap.is_enabled) {
    *is_enabled = 0;
    return;
  }

  row    = (row    + tilemap.offset_y                           ) % FRAME_BUFFER_HEIGHT;
  column = (column + tilemap.offset_x * (tilemap.use_80x32 ? 1 : 2)) % FRAME_BUFFER_WIDTH;

  const int is_clipped = \
    row        < tilemap.clip_y1 || row        > tilemap.clip_y2 ||
    column / 4 < tilemap.clip_x1 || column / 4 > tilemap.clip_x2;

  const u16_t map_offset     = tilemap_map_offset_get(row, column);
  const u8_t  attribute      = tilemap_attribute_get(row, column);
  const u8_t  tile           = tilemap.bank5[map_offset] | (tilemap.use_512_tiles ? (attribute & 0x01) << 8 : 0);
  const u8_t  def_row        = row    % 8;
  const u8_t  def_column     = (column / (tilemap.use_80x32 ? 1 : 2)) % 8;
  const u16_t def_offset     = tilemap.use_text_mode
    ? (tilemap.definitions_base_address + (tile *  8 + def_row))
    : (tilemap.definitions_base_address + (tile * 32 + def_row * 4 + def_column / 2));
  const u8_t  pattern        = tilemap.bank5[def_offset];
  const u8_t  palette_offset = attribute & (tilemap.use_text_mode ? 0xFE : 0xF0);
  const u8_t  palette_index  = (tilemap.use_text_mode
                                ? ((pattern & (0x80 >> def_column)) ? 1 : 0)
                                : ((def_column & 0x01) ? (pattern & 0x0F) : (pattern >> 4)));
  const int   is_transparent = palette_index == tilemap.transparency_index;

  *is_enabled        = 1;
  *is_pixel_enabled  = !(is_clipped || is_transparent);
  *is_pixel_below    = tilemap.use_512_tiles ? 0 : (attribute & 1);
  *is_pixel_textmode = tilemap.use_text_mode;
  *rgb               = palette_read_inline(tilemap.palette, palette_offset | palette_index);
}


void tilemap_offset_x_msb_write(u8_t value) {
  tilemap.offset_x = (value << 8) | (tilemap.offset_x & 0x00FF);
}


void tilemap_offset_x_lsb_write(u8_t value) {
  tilemap.offset_x = (tilemap.offset_x & 0xFF00) | value;
}


void tilemap_offset_y_write(u8_t value) {
  tilemap.offset_y = value;
}


void tilemap_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  tilemap.clip_x1 = x1;
  tilemap.clip_x2 = x2;
  tilemap.clip_y1 = y1;
  tilemap.clip_y2 = y2;
}
