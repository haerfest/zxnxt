#ifndef __DAC_H
#define __DAC_H


#include "defs.h"


#define DAC_A  1
#define DAC_B  2
#define DAC_C  4
#define DAC_D  8


int  dac_init(void);
void dac_finit(void);
void dac_reset(reset_t reset);
void dac_enable(int enable);
void dac_write(u16_t address, u8_t value);
void dac_mirror_write(u8_t dac_mask, u8_t value);


#endif  /* __DAC_H */
