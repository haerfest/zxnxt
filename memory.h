#ifndef __MEMORY_H
#define __MEMORY_H


#include "defs.h"


int  memory_init(void);
void memory_finit(void);
u8_t memory_read(u16_t address);

#endif  /* __MEMORY_H */
