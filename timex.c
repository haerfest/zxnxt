#include <stdio.h>
#include "defs.h"


int timex_init(void) {
  return 0;
}


void timex_finit(void) {
}


u8_t timex_read(u16_t address) {
  fprintf(stderr, "timex: unimplemented read from $%04X\n", address);
  return 0xFF;
}


void timex_write(u16_t address, u8_t value) {
  fprintf(stderr, "timex: unimplemented write of $%02X to $%04X\n", value, address);
}
