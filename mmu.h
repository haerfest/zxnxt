#ifndef __MMU_H
#define __MMU_H


#include "defs.h"


int  mmu_init(void);
void mmu_finit(void);
u8_t mmu_page_get(u8_t slot);
void mmu_page_set(u8_t slot, u8_t page);
u8_t mmu_read(u16_t address);
void mmu_write(u16_t address, u8_t value);
void mmu_select_rom(u8_t rom);


#endif  /* __MMU_H */
