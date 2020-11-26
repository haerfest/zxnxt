#ifndef __IO_H
#define __IO_H


#include "defs.h"


int  io_init(void);
void io_finit(void);
u8_t io_read(u16_t address);
void io_write(u16_t address, u8_t value);


#endif  /* __IO_H */
