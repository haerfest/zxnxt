#ifndef __ROM_H
#define __ROM_H


#include "defs.h"


int  rom_init(u8_t* rom, u8_t* altrom0, u8_t* altrom1);
void rom_finit(void);
int  rom_read(u16_t address, u8_t* value);
int  rom_write(u16_t address, u8_t value);
u8_t rom_select(u8_t rom);
u8_t rom_selected(void);
void rom_configure_altrom(int enable, int during_writes, u8_t lock_rom);


#endif  /* __ROM_H */
