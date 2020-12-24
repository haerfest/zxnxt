#include "defs.h"
#include "utils.h"


typedef struct {
  u8_t* rom;
  u8_t* altrom0;
  u8_t* altrom1;
} rom_t;


static rom_t self;


int rom_init(u8_t* rom, u8_t* altrom0, u8_t* altrom1) {
  self.rom     = rom;
  self.altrom0 = altrom0;
  self.altrom1 = altrom1;

  if (utils_load_rom("enNextZX.rom", 64 * 1024, self.rom) != 0) {
    return -1;
  }

  return 0;
}


void rom_finit(void) {
}


int rom_read(u16_t address, u8_t* value) {
  return -1;
}


int rom_write(u16_t address, u8_t value) {
  return -1;
}


u8_t rom_get(void) {
  return 0;
}
