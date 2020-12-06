#include <stdio.h>
#include "defs.h"
#include "utils.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 */


#define ROM_SIZE (8 * 1024)


typedef struct {
  u8_t* rom;
} divmmc_t;


static divmmc_t divmmc;


int divmmc_init(void) {
  divmmc.rom = malloc(ROM_SIZE);
  if (divmmc.rom == NULL) {
    fprintf(stderr, "divmmc: out of memory\n");
    return -1;
  }

  if (utils_load_rom("enNxtmmc.rom", ROM_SIZE, divmmc.rom) != 0) {
    return -1;
  }

  return 0;
}


void divmmc_finit(void) {
  if (divmmc.rom != NULL) {
    free(divmmc.rom);
    divmmc.rom = NULL;
  }
}


u8_t divmmc_control_read(u16_t address) {
  fprintf(stderr, "divmmc: unimplemented read from $%04X\n", address);
  return 0x55;
}


void divmmc_control_write(u16_t address, u8_t value) {
  fprintf(stderr, "divmmc: unimplemented write of $%02X to $%04X\n", value, address);
}
