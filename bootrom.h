#ifndef __BOOTROM_H
#define __BOOTROM_H


#include "defs.h"


int  bootrom_init(u8_t* sram);
void bootrom_finit(void);
int  bootrom_is_active(void);
void bootrom_activate(void);
void bootrom_deactivate(void);
u8_t bootrom_read(u16_t address);
void bootrom_write(u16_t address, u8_t value);


#endif  /* __BOOTROM_H */
