#include "bootrom.h"
#include "config.h"
#include "defs.h"
#include "divmmc.h"
#include "log.h"
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
  u8_t* sram;
} memory_t;


static memory_t self;


int memory_init(void) {
  size_t i;

  self.sram = malloc(RAM_SIZE);
  if (self.sram == NULL) {
    log_err("memory: out of memory\n");
    return -1;
  }

  for (i = 0; i < RAM_SIZE; i++) {
    self.sram[i] = rand() % 256;
  }

  return 0;
}


void memory_finit(void) {
  if (self.sram != NULL) {
    free(self.sram);
    self.sram = NULL;
  }
}


/**
 * Memory decode order:
 *
 * 0-16k:
 *   1. bootrom
 *   2. machine config mapping
 *   3. multiface
 *   4. divmmc
 *   5. layer 2 mapping
 *   6. mmu
 *   7. romcs expansion bus
 *   8. rom
 *
 * 16k-48k:
 *   1. layer 2 mapping
 *   2. mmu
 *
 * 48k-64k:
 *   1. mmu
 */
u8_t memory_read(u16_t address) {
  u8_t value;
  
  if (address < 0x4000) {
    if (bootrom_read(address, &value) == 0 ||
        config_read(address, &value)  == 0 ||
        divmmc_read(address, &value)  == 0 ||
        mmu_read(address, &value)     == 0 ||
        rom_read(address, &value)     == 0) {
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

  log_err("memory: cannot read from $%04X\n", address);
  return 0xFF;
}


void memory_write(u16_t address, u8_t value) {
  if (address < 0x4000) {
    if (bootrom_write(address, value) == 0 ||
        config_write(address, value)  == 0 ||
        divmmc_write(address, value)  == 0 ||
        mmu_write(address, value)     == 0 ||
        rom_write(address, value)     == 0) {
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


u8_t* memory_sram(void) {
  return self.sram;
}
