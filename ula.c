#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "ula.h"


typedef struct {
  mmu_bank_t           display_bank;
  ula_display_timing_t display_timing;
} ula_t;


static ula_t ula;


int ula_init(void) {
  ula.display_bank   = 5;
  ula.display_timing = E_ULA_DISPLAY_TIMING_ZX_48K;

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


void ula_display_timing_set(ula_display_timing_t timing) {
  const char* display_timing[8] = {
    "internal use",
    "ZX Spectrum 48K",
    "ZX Spectrum 128K/+2",
    "ZX Spectrum +2A/+2B/+3",
    "Pentagon",
    "invalid (5)",
    "invalid (6)",
    "invalid (7)"
  };

  ula.display_timing = timing;
  fprintf(stderr, "ula: display timing set to %s\n", display_timing[timing]);
}
