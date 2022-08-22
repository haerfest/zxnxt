#include <string.h>
#include "defs.h"
#include "io.h"
#include "log.h"
#include "memory.h"
#include "mf.h"
#include "paging.h"


/**
 * See:
 * - https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/device/multiface.vhd
 * - http://www.1000bit.it/support/manuali/sinclair/zxspectrum/multiface/multiface1.html
 * - https://k1.spdns.de/Vintage/Sinclair/82/Peripherals/Multiface%20I%2C%20128%2C%20and%20%2B3%20(Romantic%20Robot)/
 */


typedef struct self_t {
  u8_t*     sram;
  mf_type_t type;
  int       is_invisible;
  int       is_enabled;
} self_t;


static self_t self;


int mf_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));
  self.sram = sram;
  mf_reset(E_RESET_HARD);
  return 0;
}


void mf_finit(void) {
}


void mf_reset(reset_t reset) {
  if (reset == E_RESET_HARD) {
    self.type = E_MF_TYPE_PLUS_3;
  }

  self.is_invisible  = self.type != E_MF_TYPE_1;
  self.is_enabled    = 0;
  memory_refresh_accessors(0, 2);
}


void mf_activate(void) {
  self.is_invisible = 0;
  self.is_enabled   = 1;
  memory_refresh_accessors(0, 2);
}


void mf_deactivate(void) {
  self.is_enabled = 0;
  memory_refresh_accessors(0, 2);
}


int mf_is_active(void) {
  return self.is_enabled;
}


u8_t mf_enable_read(u16_t address) {
  self.is_enabled = !self.is_invisible;
  memory_refresh_accessors(0, 2);

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

  return 0xFF;
}


void mf_enable_write(u16_t address, u8_t value) {
  if (self.type == E_MF_TYPE_PLUS_3) {
    self.is_invisible = 1;
  }
}


u8_t mf_disable_read(u16_t address) {
  self.is_enabled = 0;
  memory_refresh_accessors(0, 2);

  return 0xFF;
}


void mf_disable_write(u16_t address, u8_t value) {
  if (self.type != E_MF_TYPE_PLUS_3 && self.type != E_MF_TYPE_1) {
    self.is_invisible = 1;
  }
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


void mf_type_set(mf_type_t type) {
  self.type = type;

  switch (self.type) {
    case E_MF_TYPE_PLUS_3:
      io_mf_ports_set(0x3F, 0xBF);
      break;
    
    case E_MF_TYPE_128_V87_2:
      io_mf_ports_set(0xBF, 0x3F);
      break;

    case E_MF_TYPE_128_V87_12:
    case E_MF_TYPE_1:
      io_mf_ports_set(0x9F, 0x1F);
      break;
  }
}


mf_type_t mf_type_get(void) {
  return self.type;
}
