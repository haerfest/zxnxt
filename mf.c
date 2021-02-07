#include <string.h>
#include "defs.h"
#include "log.h"
#include "memory.h"
#include "paging.h"


/**
 * See https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/device/multiface.vhd
 */


typedef struct {
  u8_t* sram;
  int   is_invisible;
  int   is_enabled;
} self_t;


static self_t self;


int mf_init(u8_t* sram) {
  self.sram         = sram;
  self.is_invisible = 1;
  self.is_enabled   = 0;

  return 0;
}


void mf_finit(void) {
}


void mf_activate(void) {
  self.is_enabled   = 1;
  self.is_invisible = 0;

  memory_refresh_accessors(0, 2);
}


int mf_is_active(void) {
  return self.is_enabled;
}


u8_t mf_enable_read(u16_t address) {
  if (!self.is_invisible) {
    switch (address >> 8) {
      case 0x1F:
        return paging_spectrum_plus_3_paging_read();

      case 0x7F:
        return paging_spectrum_128k_paging_read();

      default:
        break;
    }
  }

  if (self.is_invisible == 0) {
    self.is_enabled = 1;
  } else {
    self.is_enabled = 0;
  }

  memory_refresh_accessors(0, 2);

  return 0xFF;
}


void mf_enable_write(u16_t address, u8_t value) {
}


u8_t mf_disable_read(u16_t address) {
  self.is_enabled = 0;
  memory_refresh_accessors(0, 2);

  return 0xFF;
}


void mf_disable_write(u16_t address, u8_t value) {
  self.is_invisible = 1;
}


u8_t mf_rom_read(u16_t address) {
  return self.sram[MEMORY_RAM_OFFSET_MF_ROM + address];
}


void mf_rom_write(u16_t address, u8_t value) {
  log_wrn("mf: unimplemented write of $%02X to MF ROM address $%04X\n", value, address);
}


u8_t mf_ram_read(u16_t address) {
  return self.sram[MEMORY_RAM_OFFSET_MF_RAM + address - 0x2000];
}


void mf_ram_write(u16_t address, u8_t value) {
  self.sram[MEMORY_RAM_OFFSET_MF_RAM + address - 0x2000] = value;
}
