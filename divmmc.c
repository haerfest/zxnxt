#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "utils.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 */


#define BANK_SIZE  (8 * 1024)
#define BANK_START 0x2000
#define ROM_SIZE   (8 * 1024)


typedef struct {
  int   conmem_enabled;
  int   bank_number;
  u8_t* bank_pointer;
  u8_t* memory_pointer;
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

  divmmc.conmem_enabled = 0;
  divmmc.memory_pointer = mmu_divmmc_get();
  divmmc.bank_number    = 0;
  divmmc.bank_pointer   = &divmmc.memory_pointer[divmmc.bank_number * BANK_SIZE];

  return 0;
}


void divmmc_finit(void) {
  if (divmmc.rom != NULL) {
    free(divmmc.rom);
    divmmc.rom = NULL;
  }
}


u8_t divmmc_read(u16_t address) {
  if (divmmc.conmem_enabled) {
    if (address < ROM_SIZE) {
      return divmmc.rom[address];
    }

    if (address >= BANK_START && address < BANK_START + BANK_SIZE) {
      return divmmc.bank_pointer[address - BANK_START];
    }
  }

  return mmu_read(address);
}


void divmmc_write(u16_t address, u8_t value) {
  if (divmmc.conmem_enabled) {
    if (address < ROM_SIZE) {
      return;
    }

    if (address >= BANK_START && address < BANK_START + BANK_SIZE) {
      divmmc.bank_pointer[address - BANK_START] = value;
      return;
    }
  }

  mmu_write(address, value);
}


u8_t divmmc_control_read(u16_t address) {
  return 0x55;
}


void divmmc_control_write(u16_t address, u8_t value) {
  divmmc.conmem_enabled = value & 0x80;
  divmmc.bank_number    = value & 0x03;
  divmmc.bank_pointer   = &divmmc.memory_pointer[divmmc.bank_number * BANK_SIZE];

  if (value & 0x40) {
    fprintf(stderr, "divmmc: MAPRAM functionality not implemented\n");
  }
}
