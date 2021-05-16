#include <string.h>
#include "tilemap.h"


typedef struct {
  u8_t* sram;
  u8_t  default_attribute;
  u8_t  control;
  u16_t definitions_base_address;
  u16_t tilemap_base_address;
} tilemap_t;


static tilemap_t self;


int tilemap_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram                     = sram;
  self.definitions_base_address = 0x4C00;
  self.tilemap_base_address     = 0x6C00;

  return 0;
}


void tilemap_finit(void) {
}


void tilemap_tilemap_control_write(u8_t value) {
}


void tilemap_default_tilemap_attribute_write(u8_t value) {
}


void tilemap_tilemap_base_address_write(u8_t value) {
  self.tilemap_base_address = (value & 0x3F) << 8;
}


void tilemap_tilemap_tile_definitions_address_write(u8_t value) {
  self.definitions_base_address = (value & 0x3F) << 8;
}
