#include <string.h>
#include "defs.h"
#include "log.h"
#include "memory.h"


/**
 * See https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/blob/master/cores/zxnext/src/device/multiface.vhd
 */


typedef struct {
  u8_t* sram;
  int   is_active;
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
  self.is_active = 1;
}


int mf_is_active(void) {
  return self.is_active;
}


u8_t mf_enable_read(u16_t address) {
  log_dbg("mf: unimplemented read from ENABLE $%04X\n", address);
  return 0xFF;
}


void mf_enable_write(u16_t address, u8_t value) {
  log_dbg("mf: unimplemented write of $%02X to ENABLE $%04X\n", value, address);
}


u8_t mf_disable_read(u16_t address) {
  log_dbg("mf: unimplemented read from DISABLE $%04X\n", address);
  return 0xFF;
}


void mf_disable_write(u16_t address, u8_t value) {
  log_dbg("mf: unimplemented write of $%02X to DISABLE $%04X\n", value, address);
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
