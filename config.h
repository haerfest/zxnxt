#ifndef __CONFIG_H
#define __CONFIG_H


#include "defs.h"


int  config_init(u8_t* sram);
void config_finit(void);
int  config_is_active(void);
void config_activate(void);
void config_deactivate(void);
u8_t config_read(u16_t address);
void config_write(u16_t address, u8_t value);
void config_set_rom_ram_bank(u8_t bank);


#endif  /* __CONFIG_H */
