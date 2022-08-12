#ifndef __DEBUG_H
#define __DEBUG_H


#include "defs.h"


int  debug_init(void);
void debug_finit(void);
int  debug_enter(void);
int  debug_is_breakpoint(u16_t address);


#endif  /* __DEBUG_H */