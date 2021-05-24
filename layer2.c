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
  int   is_visible;
  int   is_readable;
  int   is_writable;
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
  layer2_access_write(0);
  layer2_active_bank_write(8);
  layer2_shadow_bank_write(11);
}


void layer2_access_write(u8_t value) {
  log_dbg("layer2: access write $%02X\n", value);

  self.is_readable = value & 0x04;
  self.is_visible  = value & 0x02;
  self.is_writable = value & 0x01;
}


void layer2_control_write(u8_t value) {
  log_dbg("layer2: control write $%02X\n", value);
}


void layer2_active_bank_write(u8_t bank) {
  log_dbg("layer2: active bank write $%02X\n", bank);
  if (bank != self.active_bank) {
    self.active_bank = bank;
  }
}


void layer2_shadow_bank_write(u8_t bank) {
  log_dbg("layer2: shadow bank write $%02X\n", bank);
  if (bank != self.shadow_bank) {
    self.shadow_bank = bank;
  }
}


void layer2_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  if (!self.is_visible) {
    *is_transparent = 1;
    return;
  }

  /* TODO Return colour! */
  *is_transparent = 1;
}


int layer2_is_readable(void) {
  return self.is_readable;
}


int layer2_is_writable(void) {
  return self.is_writable;
}


u8_t layer2_read(u16_t address) {
  return 0xFF;
}


void layer2_write(u16_t address, u8_t value) {
}
