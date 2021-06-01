#include "defs.h"
#include "log.h"


int dac_init(void) {
  return 0;
}


void dac_finit(void) {
}


u8_t dac_read(u16_t address) {
  log_wrn("dac: unimplemented read from $%04X\n", address);
  return 0;
}


void dac_write(u16_t address, u8_t value) {
  log_wrn("dac: unimplemented write of $%02X to $%04X\n", value, address);
}
