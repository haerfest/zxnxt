#ifndef __ALTROM_H
#define __ALTROM_H


#include "defs.h"


typedef enum {
  E_ALTROM_128K_EDITOR = 0,
  E_ALTROM_48K_BASIC
} altrom_t;


int  altrom_init(u8_t* sram);
void altrom_finit(void);
u8_t altrom_read(u16_t address);
void altrom_write(u16_t address, u8_t value);
int  altrom_is_active_on_read(void);
int  altrom_is_active_on_write(void);
void altrom_activate(int active, int on_write);
void altrom_set_machine_type(machine_type_t machine_type);
void altrom_select(u8_t rom);
void altrom_lock(u8_t rom);


#endif  /* __ALTROM_H */
