#include <stdio.h>
#include "defs.h"


int  io_init(void) {
  return 0;
}


void io_finit(void) {
}


u8_t io_read(u16_t address) {
  fprintf(stderr, "io_read(%04Xh)\n", address);
  return 0xFF;
}


void io_write(u16_t address, u8_t value) {
  fprintf(stderr, "io_write(%04Xh,%02Xh)\n", address, value);
}
