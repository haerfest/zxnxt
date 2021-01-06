#ifndef __MMU_H
#define __MMU_H


#include "defs.h"


#define MMU_ROM_PAGE  0xFF
#define MMU_ROM_BANK  MMU_ROM_PAGE


int   mmu_init(u8_t* sram);
void  mmu_finit(void);
u8_t  mmu_page_get(u8_t slot);
void  mmu_page_set(u8_t slot, u8_t page);
void  mmu_bank_set(u8_t slot, u8_t bank);
u8_t  mmu_read(u16_t address);
void  mmu_write(u16_t address, u8_t value);


#endif  /* __MMU_H */
