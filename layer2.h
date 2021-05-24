#ifndef __LAYER2_H
#define __LAYER2_H


#include "defs.h"


int  layer2_init(u8_t* sram);
void layer2_finit(void);
void layer2_reset(void);
void layer2_access_write(u8_t value);
void layer2_control_write(u8_t value);
void layer2_active_bank_write(u8_t bank);
void layer2_shadow_bank_write(u8_t bank);


#endif  /* __LAYER2_H */
