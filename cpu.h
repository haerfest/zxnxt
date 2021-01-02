#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


int  cpu_init(void);
void cpu_finit(void);
void cpu_reset(void);
int  cpu_run(u32_t n_instructions);
void cpu_irq(u32_t duration);


#endif  /* __CPU_H */
