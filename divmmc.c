#include <stdio.h>
#include "defs.h"


int divmmc_init(void) {
  return 0;
}


void divmmc_finit(void) {
}


u8_t divmmc_control_read(void) {
  fprintf(stderr, "divmmc: unimplemented control read\n");
  return 0x55;
}


void divmmc_control_write(u8_t value) {
  fprintf(stderr, "divmmc: unimplemented control write %02Xh\n", value);
}
