#ifndef __CONFIG_H
#define __CONFIG_H


#include "defs.h"


int  config_init(u8_t* sram);
void config_finit(void);
int  config_read(u16_t address, u8_t* value);
int  config_write(u16_t address, u8_t value);


#endif  /* __CONFIG_H */
