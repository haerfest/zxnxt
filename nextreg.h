#ifndef __NEXTREG_H
#define __NEXTREG_H


#include "defs.h"


int  nextreg_init(void);
void nextreg_finit(void);
void nextreg_data_write(u16_t address, u8_t value);
u8_t nextreg_data_read(u16_t address);
void nextreg_select_write(u16_t address, u8_t value);
u8_t nextreg_select_read(u16_t address);


#endif  /* __NEXTREG_H */
