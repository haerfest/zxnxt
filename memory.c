#include <stdio.h>
#include "defs.h"
#include "divmmc.h"
#include "mmu.h"
#include "memory.h"
#include "rom.h"
#include "utils.h"


/**
 * See:
 * - https://wiki.specnext.dev/Memory_map.
 * - https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/zxnext.vhd
 */


#define RAM_SIZE  (2 * 1024 * 1024)


typedef struct {
  u8_t* ram;
} memory_t;


static memory_t self;


int memory_init(void) {
  FILE*  fp;
  size_t i;

  self.ram = malloc(RAM_SIZE);
  if (self.ram == NULL) {
    fprintf(stderr, "memory: out of memory\n");
    return -1;
  }

  for (i = 0; i < RAM_SIZE; i++) {
    self.ram[i] = rand() % 256;
  }

  return 0;
}


void memory_finit(void) {
  if (self.ram != NULL) {
    free(self.ram);
    self.ram = NULL;
  }
}


u8_t memory_read(u16_t address) {
  u8_t value;
  
  if (address < 0x4000) {
    if (divmmc_read(address, &value) == 0 ||
        mmu_read(address, &value)    == 0 ||
        rom_read(address, &value)    == 0) {
      return value;
    }
  } else if (address < 0xC000) {
    if (mmu_read(address, &value) == 0) {
      return value;
    }
  } else {
    if (mmu_read(address, &value) == 0) {
      return value;
    }
  }

  fprintf(stderr, "memory: cannot read from $%04X\n", address);
  return 0xFF;
}


void memory_write(u16_t address, u8_t value) {
  if (address < 0x4000) {
    if (divmmc_write(address, value) == 0 ||
        mmu_write(address, value)    == 0 ||
        rom_write(address, value)    == 0) {
      return;
    }
  } else if (address < 0xC000) {
    if (mmu_write(address, value) == 0) {
      return;
    }
  } else {
    if (mmu_write(address, value) == 0) {
      return;
    }
  }
}


u8_t* memory_pointer(u32_t address) {
  return &self.ram[address];
}


void memory_mapping_mode_write(u8_t value) {
  const u8_t mode = value & 0x03;

  if (mode != 0x00) {
    fprintf(stderr, "Memory mapping mode %u not supported\n", value);
  }
}


static void memory_all_ram(u8_t slot_1_bank_16k, u8_t slot_2_bank_16k, u8_t slot_3_bank_16k, u8_t slot_4_bank_16k) {
  mmu_bank_set(1, slot_1_bank_16k);
  mmu_bank_set(2, slot_2_bank_16k);
  mmu_bank_set(3, slot_3_bank_16k);
  mmu_bank_set(4, slot_4_bank_16k);
}


/*
 * 0x8E (142) => Spectrum Memory Mapping
 * (R/W)
 *   bit 7 = port 0xdffd bit 0         \  RAM
 *   bits 6:4 = port 0x7ffd bits 2:0   /  bank 0-15
 * R bit 3 = 1
 * W bit 3 = 1 to change RAM bank, 0 = no change to mmu6 / mmu7 / RAM bank in ports 0x7ffd, 0xdffd
 *   bit 2 = port 0x1ffd bit 0            paging mode
 * If bit 2 = paging mode = 0 (normal)
 *   bit 1 = port 0x1ffd bit 2         \  ROM
 *   bit 0 = port 0x7ffd bit 4         /  select
 * If bit 2 = paging mode = 1 (special allRAM)
 *   bit 1 = port 0x1ffd bit 2         \  all
 *   bit 0 = port 0x1ffd bit 1         /  RAM
 * Writes can affect all ports 0x7ffd, 0xdffd, 0x1ffd
 * Writes always change the ROM / allRAM mapping
 * Writes immediately change the current mmu mapping as if by port write
 */
void memory_spectrum_memory_mapping_write(u8_t value) {
  const int change_bank = value & 0x08;
  const int paging_mode = value & 0x04;
  
  if (change_bank) {
    mmu_bank_set(4, value >> 4);
  }

  if (paging_mode == 0) {
    /* mmu_rom_set(value & 0x03); */
  } else {
    switch (value & 0x03) {
      case 0: memory_all_ram(0, 1, 2, 3); break;
      case 1: memory_all_ram(4, 5, 6, 7); break;
      case 2: memory_all_ram(4, 5, 6, 3); break;
      case 3: memory_all_ram(4, 7, 6, 3); break;
    }
  }
}
