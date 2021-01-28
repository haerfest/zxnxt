#include <string.h>
#include "defs.h"
#include "slu.h"
#include "ula.h"


typedef struct {
  slu_layer_priority_t layer_priority;
} self_t;


static self_t self;


int slu_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void slu_finit(void) {
}


u32_t slu_run(u32_t ticks_14mhz) {
  return ula_run(ticks_14mhz);
}


void slu_layer_priority_set(slu_layer_priority_t priority) {
  self.layer_priority = priority;
}


slu_layer_priority_t slu_layer_priority_get(void) {
  return self.layer_priority;
}
