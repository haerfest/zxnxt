#ifndef __LAYER2_H
#define __LAYER2_H


#include "defs.h"


int  layer2_init(void);
void layer2_finit(void);
u8_t layer2_read(u16_t address);
void layer2_write(u16_t address, u8_t value);


#endif  /* __LAYER2_H */
