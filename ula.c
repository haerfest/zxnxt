#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "ula.h"


typedef struct {
  mmu_bank_t              display_bank;
  ula_refresh_frequency_t refresh_frequency;
} ula_t;


static ula_t ula;


static void ula_ticks_callback(u64_t ticks, unsigned int delta) {
  
}


int ula_init(void) {
  ula.display_bank      = 5;
  ula.refresh_frequency = E_ULA_REFRESH_FREQUENCY_50HZ;

  if (clock_register_callback(ula_ticks_callback) != 0) {
    return -1;
  }

  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  fprintf(stderr, "ula: unimplemented read from %04Xh\n", address);
  return 0xFF;  /* No keys pressed. */
}


void ula_write(u16_t address, u8_t value) {
  fprintf(stderr, "ula: unimplemented write of %02Xh to %04Xh\n", value, address);
}


void ula_display_timing_set(clock_display_timing_t timing) {
  clock_display_timing_set(timing);
}


void ula_video_timing_set(clock_video_timing_t timing) {
  clock_video_timing_set(timing);
}


void ula_refresh_frequency_set(ula_refresh_frequency_t frequency) {
  const char* descriptions[] = {
    "50",
    "60"
  };

  ula.refresh_frequency = frequency;
  fprintf(stderr, "ula: refresh frequency set to %s Hz\n", descriptions[frequency]);
}
