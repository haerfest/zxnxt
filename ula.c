#include <SDL2/SDL.h>
#include <stdio.h>
#include "defs.h"
#include "mmu.h"
#include "ula.h"


/** See: https://wiki.specnext.dev/Reference_machines. */
typedef struct {
  unsigned int total_lines;
  unsigned int display_lines;
  unsigned int bottom_border_lines;
  unsigned int blanking_period_lines;
  unsigned int top_border_lines;
} ula_display_spec_t;


static const ula_display_spec_t ula_display_spec[5][2][2] = {
  /* ZX Spectrum 48K */ {
    /* VGA  */ { /* 50 Hz */ { 312, 192, 57, 14, 49 }, /* 50 Hz */ { 262, 192, 33, 14, 23 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* ZX Spectrum 128K/+2 */ {
    /* VGA  */ { /* 50 Hz */ { 311, 192, 57, 14, 48 }, /* 60 Hz */ { 261, 192, 33, 14, 22 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* ZX Spectrum +2A/+2B/+3 */ {
    /* VGA  */ { /* 50 Hz */ { 311, 192, 57, 14, 48 }, /* 60 Hz */ { 261, 192, 33, 14, 22 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  },
  /* Pentagon */ {
    /* VGA  */ { /* 50 Hz */ { 320, 192, 49, 14, 65 }, /* 60 Hz */ { 320, 192, 49, 14, 65 } },
    /* HDMI */ { /* 50 Hz */ { 312, 192, 40, 40, 40 }, /* 60 Hz */ { 262, 192, 20, 33, 17 } }
  }
};


typedef enum {
  E_ULA_DISPLAY_STATE_TOP_BORDER = 0,
  E_ULA_DISPLAY_STATE_LEFT_BORDER,
  E_ULA_DISPLAY_STATE_DISPLAY,
  E_ULA_DISPLAY_STATE_RIGHT_BORDER,
  E_ULA_DISPLAY_STATE_HSYNC,
  E_ULA_DISPLAY_STATE_BOTTOM_BORDER,
  E_ULA_DISPLAY_STATE_VSYNC
} ula_display_state_t;


typedef struct {
  mmu_bank_t              display_bank;
  ula_display_frequency_t display_frequency;
  ula_display_state_t     display_state;
  unsigned int            display_line;
  unsigned int            display_column;
} ula_t;


static ula_t ula;


static void ula_state_machine_tick(const ula_display_spec_t spec) {
  switch (ula.display_state) {
    case E_ULA_DISPLAY_STATE_TOP_BORDER:
      break;

    case E_ULA_DISPLAY_STATE_LEFT_BORDER:
      break;

    case E_ULA_DISPLAY_STATE_DISPLAY:
      break;

    case E_ULA_DISPLAY_STATE_HSYNC:
      break;

    case E_ULA_DISPLAY_STATE_RIGHT_BORDER:
      break;

    case E_ULA_DISPLAY_STATE_BOTTOM_BORDER:
      break;

    case E_ULA_DISPLAY_STATE_VSYNC:
      break;
  }
}


static void ula_ticks_callback(u64_t ticks, unsigned int delta) {
  const unsigned int       i    = clock_display_timing_get() - E_CLOCK_DISPLAY_TIMING_ZX_48K;
  const unsigned int       j    = clock_video_timing_get() != E_CLOCK_VIDEO_TIMING_HDMI;
  const unsigned int       k    = ula.display_frequency;
  const ula_display_spec_t spec = ula_display_spec[i][j][k];

  unsigned int tick;
  for (tick = 0; tick < delta; tick++) {
    ula_state_machine_tick(spec);
  }
}


int ula_init(void) {
  ula.display_bank      = 5;
  ula.display_frequency = E_ULA_DISPLAY_FREQUENCY_50HZ;
  ula.display_state     = E_ULA_DISPLAY_STATE_TOP_BORDER;
  ula.display_line      = 0;
  ula.display_column    = 0;

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


void ula_display_frequency_set(ula_display_frequency_t frequency) {
  const char* descriptions[] = {
    "50",
    "60"
  };

  ula.display_frequency = frequency;
  fprintf(stderr, "ula: display frequency set to %s Hz\n", descriptions[frequency]);
}
