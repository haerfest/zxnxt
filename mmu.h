#ifndef __MMU_H
#define __MMU_H


#include "defs.h"


int   mmu_init(u8_t* sram);
void  mmu_finit(void);
u8_t  mmu_page_get(u8_t slot);
void  mmu_page_set(u8_t slot, u8_t page);
void  mmu_bank_set(u8_t slot, u8_t bank);
int   mmu_read(u16_t address, u8_t* value);
int   mmu_write(u16_t address, u8_t value);


#endif  /* __MMU_H */
