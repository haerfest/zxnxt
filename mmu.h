#ifndef __MMU_H
#define __MMU_H


#include "defs.h"


typedef u8_t mmu_page_t;  /* An 8 KiB page. */
typedef u8_t mmu_bank_t;  /* A 16 KiB bank. */

/* For 8 KiB pages, the 64 KiB address space is divided into eight slots
 * numbered 0 to 7. */
typedef enum {
  E_MMU_PAGE_SLOT_0 = 0,
  E_MMU_PAGE_SLOT_1,
  E_MMU_PAGE_SLOT_2,
  E_MMU_PAGE_SLOT_3,
  E_MMU_PAGE_SLOT_4,
  E_MMU_PAGE_SLOT_5,
  E_MMU_PAGE_SLOT_6,
  E_MMU_PAGE_SLOT_7
} mmu_page_slot_t;

/* For 16 KiB banks, the 64 KiB address space is divided into four slots
 * numbered 1 to 4. */
typedef enum {
  E_MMU_BANK_SLOT_1 = 1,
  E_MMU_BANK_SLOT_2,
  E_MMU_BANK_SLOT_3,
  E_MMU_BANK_SLOT_4
} mmu_bank_slot_t;

typedef enum {
  E_MMU_ROM_0 = 0,
  E_MMU_ROM_1,
  E_MMU_ROM_2,
  E_MMU_ROM_3
} mmu_rom_t;


int        mmu_init(void);
void       mmu_finit(void);
u8_t*      mmu_divmmc_get(void);
mmu_page_t mmu_page_get(mmu_page_slot_t slot);
void       mmu_page_set(mmu_page_slot_t slot, mmu_page_t page);
void       mmu_bank_set(mmu_bank_slot_t slot, mmu_bank_t bank);
u8_t       mmu_read(u16_t address);
void       mmu_write(u16_t address, u8_t value);
u8_t       mmu_bank_read(mmu_bank_t bank, u16_t offset);
mmu_rom_t  mmu_rom_get(void);
void       mmu_rom_set(mmu_rom_t rom);


#endif  /* __MMU_H */
