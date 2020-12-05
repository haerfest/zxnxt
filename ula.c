#include <stdio.h>
#include "defs.h"


typedef struct {
  u8_t foo;
} ula_t;


static ula_t ula;


int ula_init(void) {
  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  fprintf(stderr, "ula: unimplemented read from %04Xh\n", address);
  return 0xFF;  /* No keys pressed. */
}


void ula_write(u16_t address, u8_t value) {
  fprintf(stderr, "ula: unimplemented write of %02Xh to %04Xh\n", value, address);
}
