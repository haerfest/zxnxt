#ifndef __LAYER2_H
#define __LAYER2_H


#include "defs.h"


int  layer2_init(u8_t* sram);
void layer2_finit(void);
u8_t layer2_read(u16_t address);
void layer2_write(u16_t address, u8_t value);
u8_t layer2_bank_active_get(void);
void layer2_bank_active_set(u8_t bank);
u8_t layer2_bank_shadow_get(void);
void layer2_bank_shadow_set(u8_t bank);


#endif  /* __LAYER2_H */
