#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "mmu.h"
#include "utils.h"


/**
 * See https://wiki.specnext.dev/Memory_map.
 */


#define MEMORY_SIZE  (2 * 1024 * 1024)
#define ROM_START    0x00000
#define DIVMMC_START 0x20000
#define RAM_START    0x40000
#define PAGE_SIZE    (8 * 1024)
#define N_SLOTS      (E_MMU_PAGE_SLOT_7 - E_MMU_PAGE_SLOT_0 + 1)
#define N_PAGES      ((MEMORY_SIZE - RAM_START) / PAGE_SIZE)
#define ROM_PAGE     0xFF
#define ROM_SIZE     (16 * 1024)


const u8_t default_pages[N_SLOTS] = {
  ROM_PAGE,  /* 0x0000 - 0x1FFF  =   0 KiB -  8 KiB  @  0x00000 */
  ROM_PAGE,  /* 0x2000 - 0x3FFF  =   8 KiB - 16 KiB  @  0x02000 */
  10,        /* 0x4000 - 0x5FFF  =  16 KiB - 24 KiB  @  0x40000 + 10 * 0x2000 = 0x54000 */
  11,        /* 0x6000 - 0x7FFF  =  24 KiB - 32 KiB  @  0x40000 + 11 * 0x2000 = 0x56000 */
  4,         /* 0x8000 - 0x9FFF  =  32 KiB - 40 KiB  @  0x40000 +  4 * 0x2000 = 0x48000 */
  5,         /* 0xA000 - 0xBFFF  =  40 KiB - 48 KiB  @  0x40000 +  5 * 0x2000 = 0x4A000 */
  0,         /* 0xC000 - 0xDFFF  =  48 KiB - 56 KiB  @  0x40000 +  0 * 0x2000 = 0x40000 */
  1          /* 0xE000 - 0xFFFF  =  56 KiB - 64 KiB  @  0x40000 +  1 * 0x2000 = 0x42000 */
};


typedef struct  {
  u8_t*      memory;
  mmu_page_t page[N_SLOTS];
  u8_t*      pointer[N_SLOTS];
  mmu_rom_t selected_rom;
} mmu_t;


static mmu_t self;


int mmu_init(void) {
  FILE*  fp;
  size_t i;

  self.memory = malloc(MEMORY_SIZE);
  if (self.memory == NULL) {
    fprintf(stderr, "mmu: out of memory\n");
    return -1;
  }

  for (i = 0; i < MEMORY_SIZE; i++) {
    self.memory[i] = rand() % 256;
  }

  if (utils_load_rom("enNextZX.rom", 64 * 1024, &self.memory[ROM_START]) != 0) {
    goto exit;
  }

  self.selected_rom = 0;

  /* TODO: Load in other ROMs:
   *       - EsxDOS
   *       - Multiface
   *       - Multiface Extra
   */

  for (u8_t slot = 0; slot < N_SLOTS; slot++) {
    mmu_page_set(slot, default_pages[slot]);
  }

  return 0;

exit:
  return -1;
}


void mmu_finit(void) {
  if (self.memory != NULL) {
    free(self.memory);
    self.memory = NULL;
  }
}


u8_t* mmu_divmmc_get(void) {
  return &self.memory[DIVMMC_START];
}


mmu_page_t mmu_page_get(mmu_page_slot_t slot) {
  return self.page[slot];
}


void mmu_page_set(mmu_page_slot_t slot, mmu_page_t page) {
  if (page < N_PAGES) {
    self.pointer[slot] = &self.memory[RAM_START + page * PAGE_SIZE];
    self.page[slot]    = page;
  } else if (page == ROM_PAGE && slot < 2) {
    self.pointer[slot] = &self.memory[ROM_START + self.selected_rom * ROM_SIZE + slot * PAGE_SIZE];
    self.page[slot]    = page;
  }

  fprintf(stderr, "mmu: slot %u contains page %u\n", slot, page);
}


void mmu_bank_set(mmu_bank_slot_t slot, mmu_bank_t bank) {
  const mmu_page_slot_t page_slot = (slot - 1) * 2;
  const mmu_page_t      page      = bank * 2;

  mmu_page_set(page_slot,     page);
  mmu_page_set(page_slot + 1, page + 1);
}


u8_t mmu_read(u16_t address) {
  const u8_t  slot   = address / PAGE_SIZE;
  const u16_t offset = address & (PAGE_SIZE - 1);
  return self.pointer[slot][offset];
}


void mmu_write(u16_t address, u8_t value) {
  const u8_t slot = address / PAGE_SIZE;
  if (self.page[slot] != ROM_PAGE) {
    /* TODO: Layer 2 can be mapped for writing behind the ROMs. */
    const u16_t offset = address & (PAGE_SIZE - 1);
    self.pointer[slot][offset] = value;
  }
}


void mmu_rom_set(mmu_rom_t rom) {
  const char* descriptions[] = {
    "128K editor and menu",
    "128K syntax checker",
    "+3DOS",
    "48K BASIC"
  };

  mmu_page_slot_t slot;

  self.selected_rom = rom & 0x03;
  fprintf(stderr, "mmu: ROM %u (%s) selected\n", self.selected_rom, descriptions[self.selected_rom]);

  for (slot = E_MMU_PAGE_SLOT_0; slot <= E_MMU_PAGE_SLOT_1; slot++) {
    if (self.page[slot] == ROM_PAGE) {
      self.pointer[slot] = &self.memory[ROM_START + self.selected_rom * ROM_SIZE + slot * PAGE_SIZE];
    }
  }
}


u8_t mmu_bank_read(mmu_bank_t bank, u16_t offset) {
  const mmu_page_t page = bank * 2;
  return self.memory[RAM_START + page * PAGE_SIZE + offset];
}
