#ifndef __DIVMMC_H
#define __DIVMMC_H


#include "defs.h"


int  divmmc_init(u8_t* rom, u8_t* ram);
void divmmc_finit(void);
int  divmmc_read(u16_t address, u8_t* value);
int  divmmc_write(u16_t address, u8_t value);
u8_t divmmc_control_read(u16_t address); 
void divmmc_control_write(u16_t address, u8_t value);


#endif  /* __DIVMMC_H */
