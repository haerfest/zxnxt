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


static void memory_select_bank_16k(u8_t slot, u8_t bank_16k) {
  const u8_t bank_8k = bank_16k * 2;

  mmu_page_write(slot * 2,     bank_8k);
  mmu_page_write(slot * 2 + 1, bank_8k + 1);
}


static void memory_all_ram(u8_t slot_0_bank_16k, u8_t slot_1_bank_16k, u8_t slot_2_bank_16k, u8_t slot_3_bank_16k) {
  memory_select_bank_16k(0, slot_0_bank_16k);
  memory_select_bank_16k(1, slot_1_bank_16k);
  memory_select_bank_16k(2, slot_2_bank_16k);
  memory_select_bank_16k(3, slot_3_bank_16k);
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
  const u8_t paging_mode = value & 0x04;
  const u8_t bank        = value >> 4;
  const int  change_bank = value & 0x08;
  
  if (change_bank) {
    memory_select_bank_16k(3, bank);
  }

  if (paging_mode == 0) {
    const u8_t rom = value & 0x03;
    mmu_select_rom(rom);
  } else {
    const u8_t configuration = value & 0x03;
    switch (configuration) {
      case 0:
        memory_all_ram(0, 1, 2, 3);
        break;

      case 1:
        memory_all_ram(4, 5, 6, 7);
        break;

      case 2:
        memory_all_ram(4, 5, 6, 3);
        break;

      default:
        memory_all_ram(4, 7, 6, 3);
        break;
    }
  }
}
