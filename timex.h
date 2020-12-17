#ifndef __TIMEX_H
#define __TIMEX_H


#include "defs.h"


int  timex_init(void);
void timex_finit(void);
u8_t timex_read(u16_t address);
void timex_write(u16_t address, u8_t value);


#endif  /* __TIMEX_H */
