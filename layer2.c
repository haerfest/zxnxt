#include "defs.h"
#include "log.h"


int layer2_init(void) {
  return 0;
}


void layer2_finit(void) {
}


u8_t layer2_read(u16_t address) {
  log_wrn("layer2: unimplemented read from $%04X\n", address);
  return 0xFF;
}


void layer2_write(u16_t address, u8_t value) {
  log_wrn("layer2: unimplemented write of $%02X to $%04X\n", value, address);
}
