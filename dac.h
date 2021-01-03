#ifndef __DAC_H
#define __DAC_H


#include "defs.h"


int  dac_init(void);
void dac_finit(void);
u8_t dac_read(u16_t address);
void dac_write(u16_t address, u8_t value);


#endif  /* __DAC_H */
