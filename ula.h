#ifndef __ULA_H
#define __ULA_H


#include "defs.h"


int  ula_init(void);
void ula_finit(void);
u8_t ula_read(void);
void ula_write(u8_t value);


#endif  /* __ULA_H */
