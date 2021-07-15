#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


typedef enum {
  E_CPU_IRQ_ULA,
  E_CPU_IRQ_LINE
} cpu_irq_t;


typedef enum {
  E_CPU_NMI_MF,
  E_CPU_NMI_DIVMMC
} cpu_nmi_t;


int   cpu_init(void);
void  cpu_finit(void);
void  cpu_step(void);
void  cpu_reset(void);
void  cpu_irq(cpu_irq_t irq, int active);
void  cpu_nmi(cpu_nmi_t nmi);
u16_t cpu_pc_get(void);


#endif  /* __CPU_H */
