#include <stdio.h>
#include "divmmc.h"
#include "nextreg.h"
#include "defs.h"
#include "layer2.h"
#include "spi.h"
#include "timex.h"
#include "ula.h"


int io_init(void) {
  return 0;
}


void io_finit(void) {
}


u8_t io_read(u16_t address) {
  if ((address & 0x0001) == 0x0000) {
    return ula_read(address);
  }

  if ((address & 0x00FF) == 0x00E3) {
    return divmmc_control_read(address);
  }

  if ((address & 0x00FF) == 0x00E7) {
    return spi_cs_read(address);
  }
  if ((address & 0x00FF) == 0x00EB) {
    return spi_data_read(address);
  }

  if ((address & 0x00FF) == 0x00FF) {
    return timex_read(address);
  }

  if (address == 0x123B) {
    return layer2_read(address);
  }

  if (address == 0x243B) {
    return nextreg_select_read(address);
  }
  if (address == 0x253B) {
    return nextreg_data_read(address);
  } 

  fprintf(stderr, "io: unimplemented read from $%04X\n", address);

  return 0xFF;
}


void io_write(u16_t address, u8_t value) {
  if ((address & 0x0001) == 0x0000) {
    ula_write(address, value);
    return;
  }

  if ((address & 0x00FF) == 0x00E3) {
    divmmc_control_write(address, value);
    return;
  }

  if ((address & 0x00FF) == 0x00E7) {
    spi_cs_write(address, value);
    return;
  }

  if ((address & 0x00FF) == 0x00EB) {
    spi_data_write(address, value);
    return;
  }

  if ((address & 0x00FF) == 0x00FF) {
    timex_write(address, value);
    return;
  }

  if (address == 0x123B) {
    layer2_write(address, value);
    return;
  }

  if (address == 0x243B) {
    nextreg_select_write(address, value);
    return;
  }
  if (address == 0x253B) {
    nextreg_data_write(address, value);
    return;
  }

  fprintf(stderr, "io: unimplemented write of $%02X to $%04X\n", value, address);
}
