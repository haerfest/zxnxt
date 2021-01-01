#ifndef __ROM_H
#define __ROM_H


#include "defs.h"


int  rom_init(u8_t* sram);
void rom_finit(void);
u8_t rom_read(u16_t address);
void rom_write(u16_t address, u8_t value);
void rom_select(u8_t rom);
void rom_set_machine_type(machine_type_t machine_type);
void rom_set_lock(u8_t lock);


#endif  /* __ROM_H */
