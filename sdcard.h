#ifndef __SDCARD_H
#define __SDCARD_H


#include "defs.h"


int  sdcard_init(void);
void sdcard_finit(void);
u8_t sdcard_read(u16_t address);
void sdcard_write(u16_t address, u8_t value);


#endif  /* __SDCARD_H */
