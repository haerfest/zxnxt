#ifndef __ROM_H
#define __ROM_H


#include "defs.h"


typedef enum {
  E_ROM_FIRST = 0,
  E_ROM_128K_EDITOR = E_ROM_FIRST,
  E_ROM_128K_SYNTAX_CHECKER,
  E_ROM_PLUS3_DOS,
  E_ROM_48K_BASIC,
  E_ROM_LAST = E_ROM_48K_BASIC
} rom_t;


int   rom_init(u8_t* sram);
void  rom_finit(void);
u8_t  rom_read(u16_t address);
void  rom_write(u16_t address, u8_t value);
void  rom_select(rom_t rom);
rom_t rom_selected(void);
void  rom_lock(rom_t rom);
void  rom_set_machine_type(machine_type_t machine_type);


#endif  /* __ROM_H */
