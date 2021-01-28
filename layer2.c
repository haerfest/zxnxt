#include <string.h>
#include "defs.h"
#include "log.h"


typedef enum {
  E_LAYER2_MAP_TYPE_FIRST_16K_IN_BOTTOM_16K = 0,
  E_LAYER2_MAP_TYPE_SECOND_16K_IN_BOTTOM_16K,
  E_LAYER2_MAP_TYPE_THIRD_16K_IN_BOTTOM_16K,
  E_LAYER2_MAP_TYPE_FIRST_48K_IN_BOTTOM_48K
} layer2_map_type;


typedef struct {
  u8_t*           sram;
  u8_t            bank_active;
  u8_t            bank_shadow;
  u8_t            bank_offset;
  int             do_use_shadow;
  layer2_map_type map_type;
  int             do_map_reads;
  int             do_map_writes;
  int             do_enable;
} self_t;


static self_t self;


int layer2_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram = sram;

  return 0;
}


void layer2_finit(void) {
}


u8_t layer2_read(u16_t address) {
  return self.map_type      << 6
       | self.do_use_shadow << 3
       | self.do_map_reads  << 2
       | self.do_enable     << 1
       | self.do_map_writes;
}


void layer2_write(u16_t address, u8_t value) {
  if (value & 0x10) {
    self.bank_offset = value & 0x07;
  } else {
    self.map_type      = value >> 6;
    self.do_use_shadow = (value & 0x08) >> 3;
    self.do_map_reads  = (value & 0x04) >> 2;
    self.do_enable     = (value & 0x02) >> 1;
    self.do_map_writes =  value & 0x01;
  }
}


u8_t layer2_bank_active_get(void) {
  return self.bank_active;
}


void layer2_bank_active_set(u8_t bank) {
  self.bank_active = bank;
}


u8_t layer2_bank_shadow_get(void) {
  return self.bank_shadow;
}


void layer2_bank_shadow_set(u8_t bank) {
  self.bank_shadow = bank;
}
