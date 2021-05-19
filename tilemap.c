#include <string.h>
#include "log.h"
#include "memory.h"
#include "palette.h"
#include "tilemap.h"


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


void tilemap_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {

  if (!self.is_enabled) {
    *is_transparent = 1;
    return;
  }

  // assume 80x32, not default attribute

  u8_t  map_row    = row    / 8;
  u8_t  map_column = column / 8;
  u16_t map_offset = self.tilemap_base_address + (map_row * 80 + map_column) * 2;
  u8_t  tile       = self.bank5[map_offset + 0];
  u8_t  attribute  = self.bank5[map_offset + 1];

  if (self.use_512_tiles) {
    tile |= (attribute & 0x01) << 8;
  }

  u8_t  def_row        = row    % 8;
  u8_t  def_column     = column % 8;
  u16_t def_offset     = self.use_text_mode
    ? (self.definitions_base_address + (tile *  8 + def_row))
    : (self.definitions_base_address + (tile * 32 + def_row * 4 + def_column / 2));
  u8_t  pattern        = self.bank5[def_offset];

  u8_t  palette_index  = self.use_text_mode
    ? ((pattern & (0x80 >> def_column)) ? 1 : 0)
    : ((def_column & 0x01) ? (pattern & 0x0F) : (pattern >> 4));
    
  u8_t  palette_offset = attribute & (self.use_text_mode ? 0xFE : 0xF0);

  palette_entry_t colour = palette_read_rgb(self.palette, palette_offset | palette_index);
  *rgba                  = colour.red << 12 | colour.green << 8 | colour.blue << 4;
  *is_transparent        = 0;
}
