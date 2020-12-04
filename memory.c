#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "memory.h"


int memory_init(void) {
  return 0;
}


void memory_finit(void) {
}


u8_t memory_read(u16_t address) {
  return mmu_read(address);
}


void memory_write(u16_t address, u8_t value) {
  mmu_write(address, value);
}


void memory_mapping_mode_write(u8_t value) {
  const u8_t mode = value & 0x03;

  if (mode != 0x00) {
    fprintf(stderr, "Memory mapping mode %u not supported\n", value);
  }
}


void memory_spectrum_memory_mapping_write(u8_t value) {
  if (value & 0x08) {
    /* Change 16 Kib bank 0-15 in slot 4. */
    const u8_t page = value >> 3;
    mmu_page_write(7, page);
    mmu_page_write(8, page + 1);
  }

  if (value & 0x04) {
    /* TODO: All RAM. */
  } else {
    /* TODO: ROM select. */
  }
}
