#include "altrom.h"
#include "defs.h"
#include "log.h"
#include "memory.h"
#include "nextreg.h"
#include "rom.h"
#include "utils.h"


typedef struct {
  u8_t*          sram;
  u8_t           selected;
  u8_t           lock;
  machine_type_t machine_type;
  u8_t*          ptr;
} rom_t;


static rom_t self;


static void rom_refresh_ptr(void) {
  int active;

  switch (self.machine_type) {
    case E_MACHINE_TYPE_ZX_48K:
      active = 0;
      break;

    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      active = (self.lock == 0) ? self.selected : self.lock;
      break;

    default:
      active = (self.lock == 0) ? (self.selected & 0x01) : (self.lock >> 1);
      break;
  }

  log_dbg("rom: selected=%d, lock=%d, active=%d\n", self.selected, self.lock, active);
  self.ptr = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_ROM + active * 16 * 1024];
}


int rom_init(u8_t* sram) {
  self.sram         = sram;
  self.selected     = 0;
  self.lock         = 0;
  self.machine_type = E_MACHINE_TYPE_CONFIG_MODE;

  rom_refresh_ptr();

  return 0;
}


void rom_finit(void) {
}


u8_t rom_read(u16_t address) {
  return self.ptr[address];
}


void rom_write(u16_t address, u8_t value) {
  log_wrn("rom: unsupported write of $%02X to $%02X\n", value, address);
}


void rom_set_machine_type(machine_type_t machine_type) {
  if (machine_type != self.machine_type) {
    self.machine_type = machine_type;
    rom_refresh_ptr();
    
    altrom_set_machine_type(machine_type);
  }
}


u8_t rom_selected(void) {
  return self.selected;
}


void rom_select(u8_t rom) {
  if (rom != self.selected) {
    self.selected = rom;
    rom_refresh_ptr();

    altrom_select(rom);
  }
}


void rom_set_lock(u8_t lock) {
  if (lock != self.lock) {
    self.lock = lock;
    rom_refresh_ptr();

    altrom_set_lock(lock);
  }
}
