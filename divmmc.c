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


static divmmc_t self;


int divmmc_init(void) {
  self.rom = malloc(ROM_SIZE);
  if (self.rom == NULL) {
    fprintf(stderr, "divmmc: out of memory\n");
    return -1;
  }

  if (utils_load_rom("enNxtmmc.rom", ROM_SIZE, self.rom) != 0) {
    return -1;
  }

  self.conmem_enabled = 0;
  self.memory_pointer = mmu_divmmc_get();
  self.bank_number    = 0;
  self.bank_pointer   = &self.memory_pointer[self.bank_number * BANK_SIZE];

  return 0;
}


void divmmc_finit(void) {
  if (self.rom != NULL) {
    free(self.rom);
    self.rom = NULL;
  }
}


u8_t divmmc_read(u16_t address) {
  if (self.conmem_enabled) {
    if (address < ROM_SIZE) {
      return self.rom[address];
    }

    if (address >= BANK_START && address < BANK_START + BANK_SIZE) {
      return self.bank_pointer[address - BANK_START];
    }
  }

  return mmu_read(address);
}


void divmmc_write(u16_t address, u8_t value) {
  if (self.conmem_enabled) {
    if (address < ROM_SIZE) {
      return;
    }

    if (address >= BANK_START && address < BANK_START + BANK_SIZE) {
      self.bank_pointer[address - BANK_START] = value;
      return;
    }
  }

  mmu_write(address, value);
}


u8_t divmmc_control_read(u16_t address) {
  return 0x55;
}


void divmmc_control_write(u16_t address, u8_t value) {
  self.conmem_enabled = value & 0x80;
  self.bank_number    = value & 0x03;
  self.bank_pointer   = &self.memory_pointer[self.bank_number * BANK_SIZE];

  if (value & 0x40) {
    fprintf(stderr, "divmmc: MAPRAM functionality not implemented\n");
  }
}
