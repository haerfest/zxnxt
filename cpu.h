#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


int   cpu_init(void);
void  cpu_finit(void);
void  cpu_reset(void);
void  cpu_step(void);
void  cpu_irq(u32_t duration);
u16_t cpu_pc_get(void);


#endif  /* __CPU_H */
