#ifndef __IO_H
#define __IO_H


#include "defs.h"


int  io_init(void);
void io_finit(void);
void io_reset(reset_t reset);
u8_t io_read(u16_t address);
void io_write(u16_t address, u8_t value);
void io_port_enable(u16_t address, int enable);
void io_mf_port_decoding_enable(int do_enable);
void io_mf_ports_set(u8_t enable, u8_t disable);


#endif  /* __IO_H */
