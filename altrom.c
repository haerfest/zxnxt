#include "altrom.h"
#include "defs.h"
#include "log.h"
#include "memory.h"


#define NOT_LOCKED  0


typedef struct self_t {
  u8_t*          rom0_128k;
  u8_t*          rom1_48k;
  u8_t*          ptr;
  int            is_active;
  int            on_write;
  rom_t          selected;
  rom_t          locked;
  machine_type_t machine_type;
} self_t;


static self_t self;


static void altrom_refresh_ptr(void) {
  switch (self.machine_type) {
    case E_MACHINE_TYPE_ZX_48K:
      /* We only have one ROM, which is the 48K BASIC. */
      self.ptr = self.rom1_48k;
      break;

    case E_MACHINE_TYPE_ZX_128K_PLUS2:
    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      if (self.locked == NOT_LOCKED) {
        /* Look at $7FFD bit 4 as if we're a 128K Spectrum: a 0 selects the
         * 128K Editor ROM, a 1 the 48K BASIC ROM. */
        self.ptr = (self.selected & 1) ? self.rom1_48k : self.rom0_128k;
      } else {
        /* Consider the ROM1 lock bit (NEXTREG $8C): if set, that selects
         * the 48K BASIC ROM, otherwise it's the 128K Editor ROM. */
        self.ptr = (self.locked   & 2) ? self.rom1_48k : self.rom0_128k;
      }
      break;

    default:
      log_wrn("altrom: locking not implemented for machine type %u\n", self.machine_type);
      return;
  }
}


int altrom_init(u8_t* sram) {
  self.rom0_128k    = &sram[MEMORY_RAM_OFFSET_ALTROM0_128K];
  self.rom1_48k     = &sram[MEMORY_RAM_OFFSET_ALTROM1_48K];
  self.is_active    = 0;
  self.on_write     = 1;
  self.selected     = E_ROM_128K_EDITOR;
  self.locked       = NOT_LOCKED;
  self.machine_type = E_MACHINE_TYPE_CONFIG_MODE;

  altrom_refresh_ptr();

  return 0;
}


void altrom_finit(void) {
}


u8_t altrom_read(u16_t address) {
  return self.ptr[address];
}


void altrom_write(u16_t address, u8_t value) {
  self.ptr[address] = value;
}


int altrom_is_active_on_read(void) {
  return self.is_active && !self.on_write;
}


int altrom_is_active_on_write(void) {
  return self.is_active && self.on_write;
}


void altrom_activate(int active, int on_write) {
  if (active != self.is_active || on_write != self.on_write) {
    self.is_active = active;
    self.on_write  = on_write;

    memory_refresh_accessors(0, 2);
  }
}


void altrom_set_machine_type(machine_type_t machine_type) {
  if (machine_type != self.machine_type) {
    self.machine_type = machine_type;
    altrom_refresh_ptr();
  }
}


void altrom_select(rom_t rom) {
  if (rom != self.selected) {
    self.selected = rom;
    altrom_refresh_ptr();
  }
}


rom_t altrom_selected(void) {
  return self.selected;
}


void altrom_lock(rom_t rom) {
  if (rom != self.locked) {
    self.locked = rom;
    altrom_refresh_ptr();
  }
}


rom_t altrom_locked(void) {
  return self.locked;
}
