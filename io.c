#include <stdio.h>
#include "divmmc.h"
#include "nextreg.h"
#include "defs.h"


int io_init(void) {
  return 0;
}


void io_finit(void) {
}


u8_t io_read(u16_t address) {
  if ((address & 0xFF) == 0xE3) {
    return divmmc_control_read();
  }

  if (address == 0x243B) {
    return nextreg_select_read();
  }
  if (address == 0x253B) {
    return nextreg_data_read();
  } 

  fprintf(stderr, "io: read %04Xh\n", address);

  return 0xFF;
}


void io_write(u16_t address, u8_t value) {
  if ((address & 0xFF) == 0xE3) {
    divmmc_control_write(value);
    return;
  }

  if (address == 0x243B) {
    nextreg_select_write(value);
    return;
  }
  if (address == 0x253B) {
    nextreg_data_write(value);
    return;
  }

  fprintf(stderr, "io: write %02Xh to %04Xh\n", value, address);
}
