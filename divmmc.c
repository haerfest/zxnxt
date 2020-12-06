#include <stdio.h>
#include "defs.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 */


int divmmc_init(void) {
  return 0;
}


void divmmc_finit(void) {
}


u8_t divmmc_control_read(u16_t address) {
  fprintf(stderr, "divmmc: unimplemented read from %04Xh\n", address);
  return 0x55;
}


void divmmc_control_write(u16_t address, u8_t value) {
  fprintf(stderr, "divmmc: unimplemented write of %02Xh to %04Xh\n", value, address);
}
