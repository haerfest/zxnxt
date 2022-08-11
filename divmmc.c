#include <string.h>
#include "defs.h"
#include "divmmc.h"
#include "log.h"
#include "memory.h"
#include "utils.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 */


#define CONMEM_ENABLED(value)  ((value) & 0x80)
#define MAPRAM_ENABLED(value)  ((value) & 0x40)
#define BANK_NUMBER(value)     ((value) & 0x0F)


typedef struct divmmc_t {
  u8_t* sram;
  u8_t* rom;
  u8_t* ram;
  u8_t  value;
  int   automap_enabled[E_DIVMMC_ADDR_LAST - E_DIVMMC_ADDR_FIRST + 1];
} divmmc_t;


static divmmc_t self;


static void divmmc_refresh_ptr(void) {
  /* We subtract 0x2000 because RAM is paged in starting at 0x2000, so all
   * addresses are offset 0x2000. This saves us a subtraction on every
   * access. */
  self.ram = &self.sram[MEMORY_RAM_OFFSET_DIVMMC_RAM + BANK_NUMBER(self.value) * 8 * 1024 - 0x2000];
}


int divmmc_init(u8_t* sram) {
  self.sram  = sram;
  self.rom   = &sram[MEMORY_RAM_OFFSET_DIVMMC_ROM];
  self.value = 0x00;

  divmmc_reset(E_RESET_HARD);
  divmmc_refresh_ptr();

  return 0;
}


void divmmc_finit(void) {
}


void divmmc_reset(reset_t reset) {
  memset(self.automap_enabled, 0, sizeof(self.automap_enabled));

  self.automap_enabled[E_DIVMMC_ADDR_0000] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_0008] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_0038] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_3DXX] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_0562] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_04C6] = 1;
  self.automap_enabled[E_DIVMMC_ADDR_0066] = 1;  /* TODO: delayed */
}


int divmmc_is_active(void) {
  return CONMEM_ENABLED(self.value);
}


u8_t divmmc_rom_read(u16_t address) {
  return self.rom[address];
}


void divmmc_rom_write(u16_t address, u8_t value) {
}


u8_t divmmc_ram_read(u16_t address) {
  return self.ram[address];
}


void divmmc_ram_write(u16_t address, u8_t value) {
  self.ram[address] = value;
}


u8_t divmmc_control_read(u16_t address) {
  return self.value;
}


void divmmc_control_write(u16_t address, u8_t value) {
  if (value != self.value)  {
    self.value = value;

    divmmc_refresh_ptr();

    if (MAPRAM_ENABLED(value)) {
      log_wrn("divmmc: MAPRAM functionality not implemented\n");
    }

    memory_refresh_accessors(0, 2);
  }
}


void divmmc_automap_enable(divmmc_addr_t address, int enable) {
  if (self.automap_enabled[address] != enable) {
    self.automap_enabled[address] = enable;
  }
  log_wrn("divmmc: automap %d %s functionality not implemented\n", address, enable ? "enable" : "disable");
}