#include "defs.h"
#include "log.h"
#include "memory.h"


typedef struct {
  u8_t* sram;
  u8_t  rom_ram_bank;
  u32_t rom_ram_bank_base;
  int   is_active;
} config_t;


static config_t self;


int config_init(u8_t* sram) {
  self.sram              = sram;
  self.rom_ram_bank      = 0;
  self.rom_ram_bank_base = 0;
  self.is_active         = 1;

  return 0;
}

void config_finit(void) {
}


int config_is_active(void) {
  return self.is_active;
}


void config_activate(void) {
  const int was_active = self.is_active;

  self.rom_ram_bank      = 0;
  self.rom_ram_bank_base = 0;
  self.is_active         = 1;

  if (!was_active) {
    log_dbg("config: activated\n");
    memory_refresh_accessors(0, 2);
  }
}


void config_deactivate(void) {
  const int was_active = self.is_active;
  
  self.is_active = 0;

  if (was_active) {
    log_dbg("config: deactivated\n");
    memory_refresh_accessors(0, 2);
  }
}


u8_t config_read(u16_t address) {
  return self.sram[self.rom_ram_bank_base + address];
}


void config_write(u16_t address, u8_t value) {
  self.sram[self.rom_ram_bank_base + address] = value;
}


void config_set_rom_ram_bank(u8_t bank) {
  self.rom_ram_bank      = bank;
  self.rom_ram_bank_base = bank * 16 * 1024;
  log_dbg("config: ROM/RAM bank in lower 16 KiB set to %u\n", self.rom_ram_bank);
}
