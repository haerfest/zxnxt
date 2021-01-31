#include <string.h>
#include "defs.h"
#include "log.h"


typedef struct {
  u8_t* sram;
} self_t;


static self_t self;


int mf_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));
  self.sram = sram;
  return 0;
}


void mf_finit(void) {
}


int mf_is_active(void) {
  return 0;
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
