#ifndef __SPRITES_H
#define __SPRITES_H


#include "defs.h"


int  sprites_init(void);
void sprites_finit(void);
void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba);


#endif  /* __SPRITES_H */
