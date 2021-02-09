#include <string.h>
#include "defs.h"
#include "log.h"
#include "memory.h"
#include "paging.h"


/**
 * See:
 * - https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/device/multiface.vhd
 * - http://www.1000bit.it/support/manuali/sinclair/zxspectrum/multiface/multiface1.html
 * - https://k1.spdns.de/Vintage/Sinclair/82/Peripherals/Multiface%20I%2C%20128%2C%20and%20%2B3%20(Romantic%20Robot)/
 */


typedef struct {
  u8_t* sram;
  int   is_visible;
  int   is_enabled;
} self_t;


static self_t self;


int mf_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));
  self.sram = sram;
  return 0;
}


void mf_finit(void) {
}


void mf_activate(void) {
  log_dbg("mf: activated\n");

  self.is_enabled = 1;
  self.is_visible = 1;

  memory_refresh_accessors(0, 2);
}


int mf_is_active(void) {
  return self.is_enabled;
}


u8_t mf_enable_read(u16_t address) {
  log_dbg("mf: read ENABLE  $%04X (is_visible=%c, is_enabled=%c)\n", address, self.is_visible ? 'Y' : 'N', self.is_enabled ? 'Y' : 'N');

  if (self.is_visible) {
    self.is_enabled = 1;
  } else {
    self.is_enabled = 0;
  }

  memory_refresh_accessors(0, 2);

  if (self.is_visible) {
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
  log_dbg("mf: write of $%02X to ENABLE  $%04X (is_visible=%c, is_enabled=%c)\n", value, address, self.is_visible ? 'Y' : 'N', self.is_enabled ? 'Y' : 'N');
}


u8_t mf_disable_read(u16_t address) {
  log_dbg("mf: read DISABLE $%04X (is_visible=%c, is_enabled=%c)\n", address, self.is_visible ? 'Y' : 'N', self.is_enabled ? 'Y' : 'N');

  self.is_enabled = 0;
  memory_refresh_accessors(0, 2);

  return 0xFF;
}


void mf_disable_write(u16_t address, u8_t value) {
  log_dbg("mf: write of $%02X to DISABLE $%04X (is_visible=%c, is_enabled=%c)\n", value, address, self.is_visible ? 'Y' : 'N', self.is_enabled ? 'Y' : 'N');

  self.is_visible = 0;
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
