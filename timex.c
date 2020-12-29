#include "defs.h"
#include "log.h"


int timex_init(void) {
  return 0;
}


void timex_finit(void) {
}


u8_t timex_read(u16_t address) {
  log_wrn("timex: unimplemented read from $%04X\n", address);
  return 0xFF;
}


void timex_write(u16_t address, u8_t value) {
  log_wrn("timex: unimplemented write of $%02X to $%04X\n", value, address);
}
