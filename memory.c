#include "altrom.h"
#include "bootrom.h"
#include "config.h"
#include "defs.h"
#include "divmmc.h"
#include "layer2.h"
#include "log.h"
#include "mf.h"
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
static reader_t pick_reader(int page) {
  if (page < 2) {
    /* Addresses 0x0000 - 0x3FFF. */
    if (bootrom_is_active()) {
      return bootrom_read;
    }
    if (config_is_active()) {
      return config_read;
    }
    if (mf_is_active()) {
      return (page == 0) ? mf_rom_read  : mf_ram_read;
    }
    if (divmmc_is_active()) {
      return (page == 0) ? divmmc_rom_read  : divmmc_ram_read;
    }
    if (layer2_is_readable(page)) {
      return layer2_read;
    }
    if (mmu_page_get(page) != MMU_ROM_PAGE) {
      return mmu_read;
    }

    return altrom_is_active_on_read() ? altrom_read  : rom_read;
  }

  if (page < 6 && layer2_is_readable(page)) {
    /* Addresses 0x4000 - 0xBFFF. */
    return layer2_read;
  }

  return mmu_read;
}


static writer_t pick_writer(int page) {
  if (page < 2) {
    /* Addresses 0x0000 - 0x3FFF. */
    if (bootrom_is_active()) {
      return bootrom_write;
    }
    if (config_is_active()) {
      return config_write;
    }
    if (mf_is_active()) {
      return (page == 0) ? mf_rom_write  : mf_ram_write;
    }
    if (divmmc_is_active()) {
      return (page == 0) ? divmmc_rom_write  : divmmc_ram_write;
    }
    if (layer2_is_writable(page)) {
      return layer2_write;
    }
    if (mmu_page_get(page) != MMU_ROM_PAGE) {
      return mmu_write;
    }

    return altrom_is_active_on_write() ? altrom_write  : rom_write;
  }

  if (page < 6 && layer2_is_writable(page)) {
    /* Addresses 0x4000 - 0xBFFF. */
    return layer2_write;
  }

  return mmu_write;
}


void memory_refresh_accessors(int page, int n_pages) {
  int i;

  for (i = page; i < page + n_pages; i++) {
    self.readers[i] = pick_reader(i);
    self.writers[i] = pick_writer(i);
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
