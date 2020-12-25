#ifndef __BOOTROM_H
#define __BOOTROM_H


#include "defs.h"


int  bootrom_init(u8_t* sram);
void bootrom_finit(void);
int  bootrom_read(u16_t address, u8_t* value);
int  bootrom_write(u16_t address, u8_t value);


#endif  /* __BOOTROM_H */
