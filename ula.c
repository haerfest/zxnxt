#include <stdio.h>
#include "defs.h"


int ula_init(void) {
  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(void) {
  fprintf(stderr, "ula: unimplemented read\n");
  return 0x55;
}


void ula_write(u8_t value) {
  fprintf(stderr, "ula: unimplemented write %02Xh\n", value);
}
