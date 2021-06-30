#include "dac.h"
#include "defs.h"
#include "log.h"


typedef struct {
  int is_enabled;
} dac_t;


static dac_t self;


int dac_init(void) {
  dac_reset(E_RESET_HARD);
  return 0;
}


void dac_finit(void) {
}


void dac_reset(reset_t reset) {
}


void dac_enable(int enable) {
  self.is_enabled = enable;
}


void dac_write(u16_t address, u8_t value) {
}


void dac_mirror_write(u8_t dac_mask, u8_t value) {
}
