#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "tilemap.h"


typedef struct {
  u8_t*  sram;
  u16_t* frame_buffer;
  u8_t   default_attribute;
  u16_t  definitions_base_address;
  u16_t  tilemap_base_address;
  u8_t   transparency_index;
  int    is_enabled;
  int    use_80x32;
  int    use_default_attribute;
  int    use_secondary_palette;
  int    use_512_tiles;
  int    force_tilemap_over_ula;
} tilemap_t;


static tilemap_t self;


int tilemap_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.frame_buffer = malloc(FRAME_BUFFER_SIZE);
  if (self.frame_buffer == NULL) {
    log_err("tilemap: out of memory\n");
    return -1;
  }

  self.sram                     = sram;
  self.definitions_base_address = 0x4C00;
  self.tilemap_base_address     = 0x6C00;
  self.transparency_index       = 0x0F;

  return 0;
}


void tilemap_finit(void) {
  if (self.frame_buffer != NULL) {
    free(self.frame_buffer);
    self.frame_buffer = NULL;
  }
}


void tilemap_tilemap_control_write(u8_t value) {
  self.is_enabled             = value & 0x80;
  self.use_80x32              = value & 0x40;
  self.use_default_attribute  = value & 0x20;
  self.use_secondary_palette  = value & 0x10;
  self.use_512_tiles          = value & 0x02;
  self.force_tilemap_over_ula = value & 0x01;
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


void tilemap_tick(u32_t beam_row, u32_t beam_column) {
  self.frame_buffer[beam_row * FRAME_BUFFER_WIDTH + beam_column] = 0xFF0000;
}


u16_t* tilemap_frame_buffer_get(void) {
  return self.frame_buffer;
}
