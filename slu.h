#ifndef __SLU_H
#define __SLU_H


#include "defs.h"


typedef enum {
  E_SLU_LAYER_PRIORITY_SLU = 0,
  E_SLU_LAYER_PRIORITY_LSU,
  E_SLU_LAYER_PRIORITY_SUL,
  E_SLU_LAYER_PRIORITY_LUS,
  E_SLU_LAYER_PRIORITY_USL,
  E_SLU_LAYER_PRIORITY_ULS,
  E_SLU_LAYER_PRIORITY_BLEND,
  E_SLU_LAYER_PRIORITY_BLEND_5
} slu_layer_priority_t;


int                  slu_init(void);
void                 slu_finit(void);
u32_t                slu_run(u32_t ticks_14mhz);
void                 slu_layer_priority_set(slu_layer_priority_t priority);
slu_layer_priority_t slu_layer_priority_get(void);


#endif  /* __SLU_H */
