#include "defs.h"
#include "nextreg.h"


typedef struct {
  u8_t* sram;
} config_t;


static config_t self;


int config_init(u8_t* sram) {
  self.sram = sram;
  return 0;
}

void config_finit(void) {
}


int config_read(u16_t address, u8_t* value) {
  if (!nextreg_is_config_mode_active()) {
    return -1;
  }

  *value = self.sram[nextreg_get_rom_ram_bank() * 16 * 1024 + address];
  return 0;
}


int config_write(u16_t address, u8_t value) {
  if (!nextreg_is_config_mode_active()) {
    return -1;
  }

  self.sram[nextreg_get_rom_ram_bank() * 16 * 1024 + address] = value;
  return 0;
}
