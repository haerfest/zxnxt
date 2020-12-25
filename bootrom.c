#include <stdio.h>
#include "defs.h"
#include "nextreg.h"
#include "utils.h"


typedef struct {
  u8_t* sram;
} bootrom_t;


static bootrom_t self;


int bootrom_init(u8_t* sram) {
  self.sram = sram;

  if (utils_load_rom("enNextZX.rom", 64 * 1024, self.sram) != 0) {
    return -1;
  }

  return 0;
}


void bootrom_finit(void) {
}


int bootrom_read(u16_t address, u8_t* value) {
  if (!nextreg_is_bootrom_active()) {
    return -1;
  }

  *value = self.sram[address];
  return 0;
}


int bootrom_write(u16_t address, u8_t value) {
  if (!nextreg_is_bootrom_active()) {
    return -1;
  }
  
  fprintf(stderr, "bootrom: attempt to write $%02X to $%04X\n", value, address);
  return -1;
}
