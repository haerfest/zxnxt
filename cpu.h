#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


int  cpu_init(void);
void cpu_finit(void);
void cpu_reset(void);
int  cpu_run(u32_t ticks, s32_t* ticks_left);
void cpu_tick(unsigned ticks);


#endif  /* __CPU_H */
