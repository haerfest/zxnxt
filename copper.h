#ifndef __COPPER_H
#define __COPPER_H


#include "defs.h"


int  copper_init(void);
void copper_finit(void);
void copper_reset(reset_t reset);
void copper_data_8bit_write(u8_t value);
void copper_data_16bit_write(u8_t value);
void copper_address_write(u8_t value);
void copper_control_write(u8_t value);
void copper_tick(u32_t beam_row, u32_t beam_column, int tick_28mhz);
void copper_irq(void);


#endif  /* __COPPER_H */
