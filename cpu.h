#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


typedef enum {
  E_CPU_NMI_REASON_NONE,
  E_CPU_NMI_REASON_MF,
  E_CPU_NMI_REASON_DIVMMC
} cpu_nmi_reason_t;


int   cpu_init(void);
void  cpu_finit(void);
void  cpu_reset(void);
void  cpu_step(void);
void  cpu_irq(u32_t duration);
void  cpu_nmi(cpu_nmi_reason_t reason);
u16_t cpu_pc_get(void);


#endif  /* __CPU_H */
