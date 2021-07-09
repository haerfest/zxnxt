#include "defs.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "ula.h"


#define PAGE_SIZE  (8 * 1024)
#define N_SLOTS    8


typedef struct  {
  u8_t* ram;
  u8_t  pages[N_SLOTS];
} mmu_t;


static mmu_t self;


int mmu_init(u8_t* sram) {
  self.ram = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM];

  mmu_reset(E_RESET_HARD);

  return 0;
}


void mmu_reset(reset_t reset) {
  mmu_page_set(0, MMU_ROM_PAGE);
  mmu_page_set(1, MMU_ROM_PAGE);
  mmu_page_set(2, 10);
  mmu_page_set(3, 11);
  mmu_page_set(4, 4);
  mmu_page_set(5, 5);
  mmu_page_set(6, 0);
  mmu_page_set(7, 1);
}


void mmu_finit(void) {
}


u8_t mmu_page_get(u8_t slot) {
  return self.pages[slot];
}


void mmu_page_set(u8_t slot, u8_t page) {
  if (page != self.pages[slot]) {
    const int was_rom = self.pages[slot] == MMU_ROM_PAGE;
    const int is_rom  = page == MMU_ROM_PAGE;

    self.pages[slot] = page;

    if (slot < 2 && was_rom != is_rom) {
      memory_refresh_accessors(slot, 1);
    }
  }
}


void mmu_bank_set(u8_t slot, u8_t bank) {
  const u8_t page_slot = (slot - 1) * 2;
  const u8_t page      = bank * 2;

  mmu_page_set(page_slot,     page);
  mmu_page_set(page_slot + 1, page + 1);
}


static u32_t mmu_translate(u16_t address) {
  const u8_t  slot   = address / PAGE_SIZE;
  const u16_t offset = address & (PAGE_SIZE - 1);
  const u8_t  bank   = self.pages[slot] / 2;

  /* Assertion: self.pages[slot] != MMU_ROM_PAGE. */

  ula_contend_bank(bank);

  return self.pages[slot] * PAGE_SIZE + offset;
}


u8_t mmu_read(u16_t address) {
  return self.ram[mmu_translate(address)];
}


void mmu_write(u16_t address, u8_t value) {
  self.ram[mmu_translate(address)] = value;
}
