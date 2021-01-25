#include "defs.h"
#include "ula.h"


int slu_init(void) {
  return 0;
}


void slu_finit(void) {
}


u32_t slu_run(u32_t ticks_14mhz) {
  return ula_run(ticks_14mhz);
}
