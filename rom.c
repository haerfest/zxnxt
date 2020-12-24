#include <stdio.h>
#include "defs.h"
#include "utils.h"


typedef enum {
  E_ALTROM_ACTIVE_READ,
  E_ALTROM_ACTIVE_WRITE
} altrom_active_t;


typedef struct {
  u8_t*           rom;
  u8_t*           altroms[2];
  u8_t            selected;
  int             altrom_enabled;
  altrom_active_t altrom_active;
  u8_t            lock_rom;
} rom_t;


static rom_t self;


int rom_init(u8_t* rom, u8_t* altrom0, u8_t* altrom1) {
  self.rom            = rom;
  self.altroms[0]     = altrom0;
  self.altroms[1]     = altrom1;
  self.altrom_enabled = 0;
  self.altrom_active  = E_ALTROM_ACTIVE_WRITE;
  self.lock_rom       = 0;
  self.selected       = 0;

  if (utils_load_rom("enNextZX.rom", 64 * 1024, self.rom) != 0) {
    return -1;
  }

  return 0;
}


void rom_finit(void) {
}


int rom_read(u16_t address, u8_t* value) {
  const u8_t selected = (self.lock_rom == 0) ? self.selected        : self.lock_rom;
  const u8_t altrom   = (self.lock_rom == 0) ? self.selected & 0x01 : 1 - (self.lock_rom >> 1);

  if (self.altrom_enabled && self.altrom_active == E_ALTROM_ACTIVE_READ) {
    *value = self.altroms[altrom][address];
  } else {
    *value = self.rom[selected * 16 * 1024 + address];
  }

  return 0;
}


int rom_write(u16_t address, u8_t value) {
  const u8_t selected = (self.lock_rom == 0) ? self.selected        : self.lock_rom;
  const u8_t altrom   = (self.lock_rom == 0) ? self.selected & 0x01 : 1 - (self.lock_rom >> 1);

  if (self.altrom_enabled && self.altrom_active == E_ALTROM_ACTIVE_WRITE) {
    self.altroms[altrom][address] = value;
  } else {
    fprintf(stderr, "rom: unimplemented write to $%04X\n", address);
  }

  return 0;
}


void rom_select(u8_t rom) {
  self.selected = rom & 0x03;
}


u8_t rom_selected(void) {
  return self.selected;
}


void rom_configure_altrom(int enable, int during_writes, u8_t lock_rom) {
  self.altrom_enabled = enable;
  self.altrom_active  = during_writes ? E_ALTROM_ACTIVE_WRITE : E_ALTROM_ACTIVE_READ;
  self.lock_rom       = lock_rom;
}
