#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "ula.h"


typedef struct {
  mmu_bank_t              display_bank;
  ula_display_timing_t    display_timing;
  ula_video_timing_t      video_timing;
  ula_refresh_frequency_t refresh_frequency;
} ula_t;


static ula_t ula;


int ula_init(void) {
  ula.display_bank      = 5;
  ula.display_timing    = E_ULA_DISPLAY_TIMING_ZX_48K;
  ula.video_timing      = E_ULA_VIDEO_TIMING_VGA_BASE;
  ula.refresh_frequency = E_ULA_REFRESH_FREQUENCY_50HZ;

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
  const char* descriptions[] = {
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
  fprintf(stderr, "ula: display timing set to %s\n", descriptions[timing]);
}


void ula_video_timing_set(ula_video_timing_t timing) {
  const char* descriptions[] = {
    "base VGA",
    "VGA setting 1",
    "VGA setting 2",
    "VGA setting 3",
    "VGA setting 4",
    "VGA setting 5",
    "VGA setting 6",
    "HDMI"
  };

  ula.video_timing = timing;
  fprintf(stderr, "ula: video timing set to %s\n", descriptions[timing]);
}


void ula_refresh_frequency_set(ula_refresh_frequency_t frequency) {
  const char* descriptions[] = {
    "50",
    "60"
  };

  ula.refresh_frequency = frequency;
  fprintf(stderr, "ula: refresh frequency set to %s Hz\n", descriptions[frequency]);
}
