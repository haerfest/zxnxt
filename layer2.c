#include <string.h>
#include "defs.h"
#include "layer2.h"
#include "log.h"


typedef struct {
  u8_t* sram;
  u8_t  access;
  u8_t  control;
  u8_t  active_bank;
  u8_t  shadow_bank;
} self_t;


static self_t self;


int layer2_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram = sram;

  layer2_reset();

  return 0;
}


void layer2_finit(void) {
}


void layer2_reset(void) {
  self.active_bank = 8;
  self.shadow_bank = 11;
}


void layer2_access_write(u8_t value) {
}


void layer2_control_write(u8_t value) {
}


void layer2_active_bank_write(u8_t bank) {
}


void layer2_shadow_bank_write(u8_t bank) {
}
