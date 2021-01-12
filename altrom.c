#include "defs.h"
#include "log.h"
#include "memory.h"


typedef struct {
  u8_t*          rom0_128k;
  u8_t*          rom1_48k;
  u8_t*          ptr;
  int            is_active;
  int            on_write;
  u8_t           selected;
  u8_t           lock;
  machine_type_t machine_type;

} self_t;


static self_t self;


static void altrom_refresh_ptr(void) {
  int alt_128;

  switch (self.machine_type) {
    case E_MACHINE_TYPE_ZX_48K:
      alt_128 = (self.lock == 1);
      break;

    default:
      alt_128 = (self.lock == 0) ? (1 - (self.selected & 0x01)) : (1 - (self.lock >> 1));
      break;
  }

  self.ptr = alt_128 ? self.rom0_128k : self.rom1_48k;

  log_dbg("altrom: ROM%s selected (machinetype=%s, lock=%d, selected=%d, active=%s, on=%s)\n",
          alt_128 ? "0 (128K)" : "1 (48K)",
          self.machine_type == E_MACHINE_TYPE_ZX_48K ? "48K" : "non-48K",
          self.lock,
          self.selected,
          self.is_active ? "yes" : "no",
          self.on_write  ? "write" : "read");
}


int altrom_init(u8_t* sram) {
  self.rom0_128k    = &sram[MEMORY_RAM_OFFSET_ALTROM0_128K];
  self.rom1_48k     = &sram[MEMORY_RAM_OFFSET_ALTROM1_48K];
  self.is_active    = 0;
  self.on_write     = 1;
  self.selected     = 0;
  self.lock         = 0;
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

#ifdef DEBUG
    if (active) {
      log_dbg("altrom: active on %s\n", on_write ? "write" : "read");
    } else {
      log_dbg("altrom: inactive\n");
    }
#endif

    memory_refresh_accessors(0, 2);
  }
}


void altrom_set_machine_type(machine_type_t machine_type) {
  if (machine_type != self.machine_type) {
    self.machine_type = machine_type;
    altrom_refresh_ptr();
  }
}


void altrom_select(u8_t rom) {
  if (rom != self.selected) {
    self.selected = rom;
    altrom_refresh_ptr();
  }
}


void altrom_set_lock(u8_t lock) {
  if (lock != self.lock) {
    self.lock = lock;
    altrom_refresh_ptr();
  }
}
