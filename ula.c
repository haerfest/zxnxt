#include <SDL2/SDL.h>
#include <time.h>
#include "audio.h"
#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "palette.h"
#include "ula.h"


/* Forward definitions for the various display modes. */
static void ula_irq(void);


#define PALETTE_OFFSET_INK      0
#define PALETTE_OFFSET_PAPER   16
#define PALETTE_OFFSET_BORDER  16


typedef enum {
  E_ULA_DISPLAY_MODE_SCREEN_0,
  E_ULA_DISPLAY_MODE_SCREEN_1,
  E_ULA_DISPLAY_MODE_HI_COLOUR,
  E_ULA_DISPLAY_MODE_HI_RES
} ula_display_mode_t;


/** See: https://wiki.specnext.dev/Reference_machines. */
typedef struct {
  unsigned int total_lines;
  unsigned int display_lines;
  unsigned int bottom_border_lines;
  unsigned int blanking_period_lines;
  unsigned int top_border_lines;
} ula_display_spec_t;


#define N_TIMINGS      (E_ULA_DISPLAY_TIMING_PENTAGON - E_ULA_DISPLAY_TIMING_INTERNAL_USE + 1)
#define N_FREQUENCIES  2

static const ula_display_spec_t ula_display_spec[N_TIMINGS][N_FREQUENCIES] = {
  /* Internal use. */ {
    /* 50 Hz */ {   0,   0,  0,  0,  0 },
    /* 60 Hz */ {   0,   0,  0,  0,  0 }
  },
  /* ZX Spectrum 48K */ {
    /* 50 Hz */ { 312, 192, 56,  8, 56 },  /* Next seems to differ? */
    /* 60 Hz */ { 262, 192, 33, 14, 23 }
  },
  /* ZX Spectrum 128K/+2 */ {
    /* 50 Hz */ { 311, 192, 57, 14, 48 },
    /* 60 Hz */ { 261, 192, 33, 14, 22 }
  },
  /* ZX Spectrum +2A/+2B/+3 */ {
    /* 50 Hz */ { 311, 192, 56,  8, 56 },
    /* 60 Hz */ { 261, 192, 33, 14, 22 }
  },
  /* Pentagon */ {
    /* 50 Hz */ { 320, 192, 49, 14, 65 },
    /* 60 Hz */ { 320, 192, 49, 14, 65 }
  }
};


typedef struct {
  u8_t*                      sram;
  const ula_display_spec_t*  display_spec;
  int                        did_display_spec_change;
  u8_t*                      display_ram;
  u8_t*                      display_ram_odd;
  ula_display_timing_t       display_timing;
  int                        display_frequency;
  ula_display_mode_t         display_mode;
  ula_display_mode_t         display_mode_requested;
  u8_t*                      attribute_ram;
  u8_t                       attribute_byte;
  u8_t                       border_colour;
  u8_t                       speaker_state;
  palette_t                  palette;
  u16_t*                     frame_buffer;
  u16_t*                     pixel;
  int                        clip_x1;
  int                        clip_x2;
  int                        clip_y1;
  int                        clip_y2;
  uint64_t                   frame_counter;
  int                        blink_state;
  s8_t                       audio_last_sample;
  ula_screen_bank_t          screen_bank;
  int                        timex_disable_ula_interrupt;
  u8_t                       hi_res_ink_colour;
  int                        do_contend;
  u32_t                      ticks_14mhz_after_irq;
  int                        is_timex_enabled;
  int                        is_7mhz_tick;
  int                        is_displaying_content;
} self_t;


static self_t self;


#include "ula_mode_x.c"
#include "ula_hi_res.c"


#define N_DISPLAY_MODES   (E_ULA_DISPLAY_MODE_HI_RES   - E_ULA_DISPLAY_MODE_SCREEN_0    + 1)


typedef void (*ula_display_mode_handler_t)(u32_t beam_row, u32_t beam_column);

const ula_display_mode_handler_t ula_display_handlers[N_DISPLAY_MODES] = {  
  ula_display_mode_screen_x,  /* E_ULA_DISPLAY_MODE_SCREEN_0 */
  ula_display_mode_screen_x,  /* E_ULA_DISPLAY_MODE_SCREEN_1 */
  ula_display_mode_screen_x,  /* E_ULA_DISPLAY_MODE_HI_COLOUR (TODO) */
  ula_display_mode_hi_res     /* E_ULA_DISPLAY_MODE_HI_RES */
};


static void ula_display_reconfigure(void) {
  /**
   * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/ports.txt
   *
   * "If the screen is located in bank 7, the ula fetches a standard spectrum
   * display ignoring modes selected in port 0xFF."
   *
   * It doesn't specify _which_ standard display, so I assume either is
   * supported.
   */
  const ula_display_mode_t mode = (self.screen_bank == E_ULA_SCREEN_BANK_7) ? (self.display_mode_requested & 1) : self.display_mode_requested;

  /* Remember the requested mode in case we honor it in bank 5. */
  self.display_mode = self.display_mode_requested;
  self.display_spec = &ula_display_spec[self.display_timing][self.display_frequency == 60];

  /* Use the effective mode, which depends on the actual bank. */
  switch (mode) {
    case E_ULA_DISPLAY_MODE_SCREEN_0:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.attribute_ram = &self.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_SCREEN_1:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024 + 0x2000];
      self.attribute_ram = &self.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_COLOUR:
      self.display_ram   = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.attribute_ram = &self.display_ram[0x2000];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_RES:
      self.display_ram     = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + self.screen_bank * 16 * 1024];
      self.display_ram_odd = &self.display_ram[0x2000];
      break;
  }
}


static void ula_irq(void) {
  if (!self.timex_disable_ula_interrupt) {
    cpu_irq(32);
  }

  self.ticks_14mhz_after_irq = 0;
}


void ula_did_complete_frame(void) {
  if ((++self.frame_counter & 15) == 0) {
    self.blink_state ^= 1;
  }

  /* Start drawing at the top again. */
  self.pixel = self.frame_buffer;

  if (self.did_display_spec_change) {
    ula_display_reconfigure();
    self.did_display_spec_change = 0;
  }
}


void ula_tick(u32_t beam_row, u32_t beam_column) {
  self.ticks_14mhz_after_irq++;

  if (self.display_mode != E_ULA_DISPLAY_MODE_HI_RES) {
    /* All other modes need a 7 MHz clock. */
    self.is_7mhz_tick = !self.is_7mhz_tick;
    if (!self.is_7mhz_tick) {
      return;
    }
  }

  ula_display_handlers[self.display_mode](beam_row, beam_column);
}


static void ula_set_display_mode(ula_display_mode_t mode) {
  self.display_mode_requested  = mode;
  self.did_display_spec_change = 1;
}


int ula_init(u8_t* sram) {
  self.frame_buffer = malloc(FRAME_BUFFER_SIZE);
  if (self.frame_buffer == NULL) {
    log_err("ula: out of memory\n");
    return -1;
  }

  self.sram                        = sram;
  self.screen_bank                 = E_ULA_SCREEN_BANK_5;
  self.display_timing              = E_ULA_DISPLAY_TIMING_ZX_48K;
  self.display_frequency           = 50;
  self.border_colour               = 0;
  self.speaker_state               = 0;
  self.palette                     = E_PALETTE_ULA_FIRST;
  self.clip_x1                     = 0;
  self.clip_x2                     = 255 * 2;
  self.clip_y1                     = 0;
  self.clip_y2                     = 191;
  self.audio_last_sample           = 0;
  self.timex_disable_ula_interrupt = 0;
  self.hi_res_ink_colour           = 0;
  self.pixel                       = self.frame_buffer;
  self.is_timex_enabled            = 0;

  ula_set_display_mode(E_ULA_DISPLAY_MODE_SCREEN_0);
  ula_display_reconfigure();

  return 0;
}


void ula_finit(void) {
}


u8_t ula_read(u16_t address) {
  return keyboard_read(address);
}


void ula_write(u16_t address, u8_t value) {
  const u8_t speaker_state = value & 0x10;
  const s8_t sample        = speaker_state ? AUDIO_MAX_VOLUME : 0;

  audio_add_sample(E_AUDIO_SOURCE_BEEPER, sample);

  self.speaker_state = speaker_state;
  self.border_colour = value & 0x07;
}


void ula_timex_video_mode_read_enable(int do_enable) {
  self.is_timex_enabled = do_enable;

  log_dbg("ula: timex port set to %s\n", do_enable ? "return video mode" : "floating bus");
}


u8_t ula_timex_read(u16_t address) {
  if (self.is_timex_enabled) {
    return self.timex_disable_ula_interrupt << 6
         | self.hi_res_ink_colour           << 3
         | self.display_mode;
  }

  if (self.is_displaying_content) {
    /* Implement floating bus behaviour. This fixes Arkanoid freezing at the
     * start of the first level and prevents flickering and slowdown in
     * Short Circuit.
     */
    return self.attribute_byte;
  }

  return 0xFF;
}


void ula_timex_write(u16_t address, u8_t value) {
  log_dbg("ula: timex write of $%02X to $%04X\n", value, address);

  self.timex_disable_ula_interrupt = (value & 0x40) >> 6;
  self.hi_res_ink_colour           = (value & 0x38) >> 3;

  switch (value & 0x07) {
    case 0x00:
      ula_set_display_mode(E_ULA_DISPLAY_MODE_SCREEN_0);
      break;

    case 0x01:
      ula_set_display_mode(E_ULA_DISPLAY_MODE_SCREEN_1);
      break;

    case 0x02:
      ula_set_display_mode(E_ULA_DISPLAY_MODE_HI_COLOUR);
      break;

    case 0x06:
      ula_set_display_mode(E_ULA_DISPLAY_MODE_HI_RES);
      break;

    default:
      break;
  }
}


ula_display_timing_t ula_display_timing_get(void) {
  return self.display_timing;
}


void ula_display_timing_set(ula_display_timing_t timing) {
#ifdef DEBUG
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
#endif

  self.display_timing          = timing;
  self.did_display_spec_change = 1;
  log_dbg("ula: display timing set to %s\n", descriptions[timing]);
}


void ula_display_frequency_set(int is_60hz) {
  const int display_frequency = is_60hz ? 60 : 50;

  if (display_frequency != self.display_frequency) {
    self.display_frequency       = display_frequency;
    self.did_display_spec_change = 1;
    log_dbg("ula: display frequency set to %d Hz\n", self.display_frequency);
  }
}


void ula_palette_set(int use_second) {
  self.palette = use_second ? E_PALETTE_ULA_SECOND : E_PALETTE_ULA_FIRST;
}


void ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;

  log_dbg("ula: clipping window set to %d <= x <= %d and %d <= y <= %d\n", self.clip_x1, self.clip_x2, self.clip_y1, self.clip_y2);
}


void ula_screen_bank_set(ula_screen_bank_t bank) {
  if (bank != self.screen_bank) {
    self.screen_bank            = bank;
    self.display_mode_requested = self.display_mode;  /* Does not change. */

    /* Changing the bank has an immediate effect. */
    ula_display_reconfigure();
  }

  log_dbg("ula: screen bank set to %u\n", bank);
}


ula_screen_bank_t ula_screen_bank_get(void) {
  return self.screen_bank;
}


int ula_contention_get(void) {
  return self.do_contend;
}


void ula_contention_set(int do_contend) {
  if (do_contend != self.do_contend) {
    self.do_contend = do_contend;
    log_inf("ula: memory contention %sabled\n", do_contend ? "en" : "dis");
  }
}


/**
 * TODO: See https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
 *
 * The contention actually starts at t-state 14335, i.e. one t-state *before*
 * the first pixel in the top-left corner is being drawn.
 */
static void ula_contend_48k(void) {
  if (self.is_displaying_content) {
    const u32_t delay[8] = {
      6, 5, 4, 3, 2, 1, 0, 0
    };
    const u32_t t_states = self.ticks_14mhz_after_irq / 4;

    clock_run(delay[(t_states % 224) % 8]);
  }
}


static void ula_contend_128k(void) {
}


static void ula_contend_plus_2(void) {
}


void ula_contend(u8_t bank) {
  if (!self.do_contend) {
    /* Contention can be disabled by writing to a Next register. */
    return;
  }

  if (clock_cpu_speed_get() != E_CLOCK_CPU_SPEED_3MHZ) {
    /* Contention only plays a role when running at 3.5 MHz. */
    return;
  }

  if (bank > 7) {
    /* Only banks 0-7 can be contended. */
    return;
  }

  switch (self.display_timing) {
    case E_ULA_DISPLAY_TIMING_ZX_48K:
      if (bank == 5) {
        /* Only bank 5 is contended. */
        ula_contend_48k();
      }
      break;

    case E_ULA_DISPLAY_TIMING_ZX_128K:
      if ((bank & 1) == 1) {
        /* Only odd banks are contended. */
        ula_contend_128k();
      }
      break;

    case E_ULA_DISPLAY_TIMING_ZX_PLUS_2A:
      if (bank > 3) {
        /* Only banks four and above are contended. */
        ula_contend_plus_2();
      }
      break;

    default:
      break;
  }
}


u8_t* ula_frame_buffer_get(void) {
  return self.frame_buffer;
}
