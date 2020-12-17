#include <stdio.h>
#include "defs.h"


int spi_init(void) {
  return 0;
}


void spi_finit(void) {
}


u8_t spi_cs_read(u16_t address) {
  fprintf(stderr, "spi: unimplemented read from CS ($%04X)\n", address);
  return 0xFF;
}


void spi_cs_write(u16_t address, u8_t value) {
  fprintf(stderr, "spi: unimplemented write of $%02X to CS ($%04X)\n", value, address);
}


u8_t spi_data_read(u16_t address) {
  fprintf(stderr, "spi: unimplemented read from DATA ($%04X)\n", address);
  return 0xFF;
}


void spi_data_write(u16_t address, u8_t value) {
  fprintf(stderr, "spi: unimplemented write of $%02X to DATA ($%04X)\n", value, address);
}
