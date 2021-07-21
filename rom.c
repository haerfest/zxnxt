#include "altrom.h"
#include "defs.h"
#include "divmmc.h"
#include "log.h"
#include "memory.h"
#include "nextreg.h"
#include "rom.h"
#include "utils.h"


#define NOT_LOCKED  0


typedef struct self_t {
  u8_t*          sram;
  rom_t          selected;
  rom_t          locked;
  machine_type_t machine_type;
  u8_t*          ptr;
} self_t;


static self_t self;


static void rom_refresh_ptr(void) {
  rom_t active;

  switch (self.machine_type) {
    case E_MACHINE_TYPE_ZX_48K:
      /* Select the 48K BASIC. */
      active = E_ROM_48K_BASIC;
      break;

    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      if (self.locked == NOT_LOCKED) {
        /* Look at $7FFD bit 4 as if we're a 128K Spectrum: a 0 selects the
         * 128K Editor ROM, a 1 the 48K BASIC ROM. */
        active = (self.selected & 1) ? E_ROM_48K_BASIC : E_ROM_128K_EDITOR;
      } else {
        /* Consider the ROM1 lock bit (NEXTREG $8C): if set, that selects
         * the 48K BASIC ROM, otherwise it's the 128K Editor ROM. */
        active = (self.locked   & 2) ? E_ROM_48K_BASIC : E_ROM_128K_EDITOR;
      }
      break;

    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      if (self.locked == NOT_LOCKED) {
        active = self.selected;
      } else {
        active = self.locked;
      }
      break;

    default:
      log_wrn("rom: locking not implemented for machine type %u\n", self.machine_type);
      return;
  }

  self.ptr = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_ROM + active * 16 * 1024];
}


int rom_init(u8_t* sram) {
  self.sram         = sram;
  self.selected     = E_ROM_128K_EDITOR;
  self.locked       = NOT_LOCKED;
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
}


void rom_set_machine_type(machine_type_t machine_type) {
  if (machine_type != self.machine_type) {
    self.machine_type = machine_type;
    rom_refresh_ptr();
    
    altrom_set_machine_type(machine_type);
  }
}


rom_t rom_selected(void) {
  return self.selected;
}


void rom_select(rom_t rom) {
  if (rom != self.selected) {
    self.selected = rom;
    rom_refresh_ptr();

    altrom_select(rom);
  }
}


void rom_lock(rom_t rom) {
  if (rom != self.locked) {
    self.locked = rom;
    rom_refresh_ptr();

    altrom_lock(rom);
  }
}
