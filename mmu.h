#ifndef __MMU_H
#define __MMU_H


#include "defs.h"


int  mmu_init(void);
void mmu_finit(void);
void mmu_page(u8_t slot, u8_t page);
u8_t mmu_read(u16_t address);
void mmu_write(u16_t address, u8_t value);


#endif  /* __MMU_H */
