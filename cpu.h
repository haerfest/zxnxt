#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


int  cpu_init(void);
void cpu_finit(void);
void cpu_reset(void);
int  cpu_run(u32_t ticks);
void cpu_irq(void);


#endif  /* __CPU_H */
