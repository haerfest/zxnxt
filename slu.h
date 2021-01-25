#ifndef __SLU_H
#define __SLU_H


#include "defs.h"


int   slu_init(void);
void  slu_finit(void);
u32_t slu_run(u32_t ticks_14mhz);


#endif  /* __SLU_H */
