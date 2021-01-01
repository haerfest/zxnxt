#include "altrom.h"
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


#define RAM_SIZE            (2 * 1024 * 1024)
#define ADDRESS_SPACE_SIZE  0x10000
#define ADDRESS_PAGE_SIZE   0x2000


typedef u8_t (*reader_t)(u16_t address);
typedef void (*writer_t)(u16_t address, u8_t value);


typedef struct {
  u8_t*    sram;
  reader_t readers[ADDRESS_SPACE_SIZE / ADDRESS_PAGE_SIZE];
  writer_t writers[ADDRESS_SPACE_SIZE / ADDRESS_PAGE_SIZE];
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
void memory_refresh_accessors(int page, int n_pages) {
  int i;

  for (i = page; i < page + n_pages; i++) {
    if (i < 2) {
      /* Addresses 0x0000 - 0x3FFF. */
      if (bootrom_is_active()) {
        self.readers[i] = bootrom_read;
        self.writers[i] = bootrom_write;
      } else if (config_is_active()) {
        self.readers[i] = config_read;
        self.writers[i] = config_write;
      } else if (divmmc_is_active()) {
        self.readers[i] = divmmc_read;
        self.writers[i] = divmmc_write;
      } else if (mmu_page_get(i) != MMU_ROM_PAGE) {
        self.readers[i] = mmu_read;
        self.writers[i] = mmu_write;
      } else {
        self.readers[i] = altrom_is_active_on_read()  ? altrom_read  : rom_read;
        self.writers[i] = altrom_is_active_on_write() ? altrom_write : rom_write;
      }
    } else {
      /* Addresses 0x4000 - 0xFFFF. */
      self.readers[i] = mmu_read;
      self.writers[i] = mmu_write;
    }
  }

  log_dbg("memory: refreshed accessors for pages %u to %u\n", page, page + n_pages - 1);
}


u8_t memory_read(u16_t address) {
  const u8_t page = address / ADDRESS_PAGE_SIZE;
  return self.readers[page](address);
}


void memory_write(u16_t address, u8_t value) {
  const u8_t page = address / ADDRESS_PAGE_SIZE;
  self.writers[page](address, value);
}


u8_t* memory_sram(void) {
  return self.sram;
}
