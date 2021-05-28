#include <string.h>
#include "log.h"
#include "memory.h"
#include "palette.h"
#include "tilemap.h"


/**
 * TODO:
 * - tile rotation
 * - tile mirroring
 * - clipping window
 */


typedef struct {
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
  int       force_tilemap_over_ula;
  palette_t palette;
  u8_t      offset_y;
  u16_t     offset_x;
  int       clip_x1;  /** In half-pixels. */
  int       clip_x2;  /** In half-pixels. */
  int       clip_y1;
  int       clip_y2;
} tilemap_t;


static tilemap_t self;


int tilemap_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.bank5                    = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];
  self.definitions_base_address = 0x0C00;
  self.tilemap_base_address     = 0x2C00;
  self.transparency_index       = 0x0F;

  return 0;
}


void tilemap_finit(void) {
}


void tilemap_tilemap_control_write(u8_t value) {
  self.is_enabled             = value & 0x80;
  self.use_80x32              = value & 0x40;
  self.use_default_attribute  = value & 0x20;
  self.use_text_mode          = value & 0x08;
  self.use_512_tiles          = value & 0x02;
  self.force_tilemap_over_ula = value & 0x01;

  self.palette = (value & 0x10) ? E_PALETTE_TILEMAP_SECOND : E_PALETTE_TILEMAP_FIRST;

  log_dbg("tilemap: set control to $%02X => %s, %dx32, %s attr, %s mode, %d tiles, %s, palette %d\n",
          value,
          self.is_enabled             ? "on"       : "off",
          self.use_80x32              ? 80         : 40,
          self.use_default_attribute  ? "default"  : "map",
          self.use_text_mode          ? "text"     : "tile",
          self.use_512_tiles          ? 512        : 256,
          self.force_tilemap_over_ula ? "tile>ula" : "ula>tile",
          self.palette                ? 2          : 1);
}


void tilemap_default_tilemap_attribute_write(u8_t value) {
  self.default_attribute = value;
}


void tilemap_tilemap_base_address_write(u8_t value) {
  self.tilemap_base_address = (value & 0x3F) << 8;
}


void tilemap_tilemap_tile_definitions_address_write(u8_t value) {
  self.definitions_base_address = (value & 0x3F) << 8;
}


void tilemap_transparency_index_write(u8_t value) {
  self.transparency_index = value;
}


static u16_t tilemap_map_offset_get(u32_t row, u32_t column) {
  const u8_t  map_row    = row    / 8;
  const u8_t  map_column = column / (self.use_80x32 ? 8 : 16);

  return self.tilemap_base_address + (map_row * (self.use_80x32 ? 80 : 40) + map_column) * (self.use_default_attribute ? 1 : 2);
}


static u8_t tilemap_attribute_get(u32_t row, u32_t column) {
  u16_t map_offset;

  if (self.use_default_attribute) {
    return self.default_attribute;
  }
  
  map_offset = tilemap_map_offset_get(row, column);
  return self.bank5[map_offset + 1];
}


void tilemap_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  *is_transparent = 1;

  if (!self.is_enabled) {
    return;
  }

  /* Honour the clipping area. */
  if (row    < self.clip_y1
   || row    > self.clip_y2
   || column < self.clip_x1
   || column > self.clip_x2) {
    return;
  }

  row    += self.offset_y;
  column += self.offset_x;

  u16_t map_offset = tilemap_map_offset_get(row, column);
  u8_t  tile       = self.bank5[map_offset];
  u8_t  attribute  = tilemap_attribute_get(row, column);

  if (self.use_512_tiles) {
    tile |= (attribute & 0x01) << 8;
  }

  u8_t  def_row        = row    % 8;
  u8_t  def_column     = (column / (self.use_80x32 ? 1 : 2)) % 8;
  u16_t def_offset     = self.use_text_mode
    ? (self.definitions_base_address + (tile *  8 + def_row))
    : (self.definitions_base_address + (tile * 32 + def_row * 4 + def_column / 2));
  u8_t  pattern        = self.bank5[def_offset];

  u8_t  palette_index  = self.use_text_mode
    ? ((pattern & (0x80 >> def_column)) ? 1 : 0)
    : ((def_column & 0x01) ? (pattern & 0x0F) : (pattern >> 4));
    
  u8_t  palette_offset = attribute & (self.use_text_mode ? 0xFE : 0xF0);

  palette_index |= palette_offset;

  if (!palette_is_msb_equal(self.palette, palette_index, self.transparency_index)) {
    *rgba           = palette_read_rgba(self.palette, palette_index);
    *is_transparent = 0;
  }
}


void tilemap_offset_x_msb_write(u8_t value) {
  self.offset_x = ((value << 8) | (self.offset_x & 0x00FF)) % (self.use_80x32 ? 640 : 320);
}


void tilemap_offset_x_lsb_write(u8_t value) {
  self.offset_x = ((self.offset_x & 0xFF00) | value) % (self.use_80x32 ? 640 : 320);
}


void tilemap_offset_y_write(u8_t value) {
  self.offset_y = value;
}


int tilemap_priority_over_ula_get(u32_t row, u32_t column) {
  if (!self.is_enabled) {
    return 0;
  }
  
  if (self.force_tilemap_over_ula) {
    return 1;
  }

  if (self.use_512_tiles) {
    return 0;
  }

  return tilemap_attribute_get(row, column) ^ 0x01;
}


void tilemap_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1 * 2;
  self.clip_x2 = x2 * 2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;

  log_dbg("tilemap: clipping window set to %d <= x <= %d and %d <= y <= %d\n", x1, x2, y1, y2);
}
