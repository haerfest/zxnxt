#include "defs.h"
#include "log.h"
#include "memory.h"
#include "utils.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 */


typedef struct {
  u8_t* rom;
  u8_t* ram;
  int   conmem_enabled;
  int   bank_number;
} divmmc_t;


static divmmc_t self;


int divmmc_init(u8_t* sram) {
  self.rom            = &sram[MEMORY_RAM_OFFSET_DIVMMC_ROM];
  self.ram            = &sram[MEMORY_RAM_OFFSET_DIVMMC_RAM];
  self.conmem_enabled = 0;
  self.bank_number    = 0;

  return 0;
}


void divmmc_finit(void) {
}


int divmmc_is_active(void) {
  return self.conmem_enabled;
}


u8_t divmmc_read(u16_t address, u8_t* value) {
  if (address < 0x2000) {
    return self.rom[address];
  }
  
  return self.ram[self.bank_number * 8 * 1024 + address - 0x2000];
}


void divmmc_write(u16_t address, u8_t value) {
  self.ram[self.bank_number * 8 * 1024 + address - 0x2000] = value;
}


u8_t divmmc_control_read(u16_t address) {
  return self.conmem_enabled << 7 | self.bank_number;
}


void divmmc_control_write(u16_t address, u8_t value) {
  if (value != (self.conmem_enabled << 7 | self.bank_number)) {
    self.conmem_enabled = value >> 7;
    self.bank_number    = value & 0x0F;

    if (self.conmem_enabled) {
      log_dbg("divmmc: CONMEM enabled, bank %d paged in\n", self.bank_number);
    } else {
      log_dbg("divmmc: CONMEM disabled\n");
    }

    if (value & 0x40) {
      log_wrn("divmmc: MAPRAM functionality not implemented\n");
    }

    memory_refresh_accessors(0, 2);
  }
}
