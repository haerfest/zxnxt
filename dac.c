#include "audio.h"
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
  self.is_enabled = 0;
}


void dac_enable(int enable) {
  self.is_enabled = enable;
}


void dac_write(u16_t address, u8_t value) {
  if (!self.is_enabled) {
    return;
  }

  log_wrn("dac: unimplemented write of $%02X to $%02X\n", value, address & 0x00FF);
}


void dac_mirror_write(u8_t dac_mask, u8_t value) {
  if (!self.is_enabled) {
    return;
  }

  if (dac_mask & DAC_A) audio_add_sample(E_AUDIO_SOURCE_DAC_A, (s8_t) value);
  if (dac_mask & DAC_B) audio_add_sample(E_AUDIO_SOURCE_DAC_B, (s8_t) value);
  if (dac_mask & DAC_C) audio_add_sample(E_AUDIO_SOURCE_DAC_C, (s8_t) value);
  if (dac_mask & DAC_D) audio_add_sample(E_AUDIO_SOURCE_DAC_D, (s8_t) value);
}
