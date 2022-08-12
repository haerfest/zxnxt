#ifndef __IO_H
#define __IO_H


#include "defs.h"


int  io_init(void);
void io_finit(void);
void io_reset(reset_t reset);
u8_t io_read(u16_t address);
void io_write(u16_t address, u8_t value);
void io_decoding_write(u8_t index, u8_t value);
void io_mf_ports_set(u8_t enable, u8_t disable);
void io_traps_enable(int enable);


#endif  /* __IO_H */
