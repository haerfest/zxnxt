#include <time.h>
#include "audio.h"
#include "clock.h"
#include "copper.h"
#include "cpu.h"
#include "defs.h"
#include "keyboard.h"
#include "log.h"
#include "main.h"
#include "memory.h"
#include "palette.h"
#include "slu.h"
#include "ula.h"


typedef enum {
  E_ULA_DISPLAY_MODE_FIRST = 0,
  E_ULA_DISPLAY_MODE_SCREEN_0 = E_ULA_DISPLAY_MODE_FIRST,
  E_ULA_DISPLAY_MODE_SCREEN_1,
  E_ULA_DISPLAY_MODE_HI_COLOUR,
  E_ULA_DISPLAY_MODE_HI_RES,
  E_ULA_DISPLAY_MODE_LO_RES,
  E_ULA_DISPLAY_MODE_LAST = E_ULA_DISPLAY_MODE_LO_RES
} ula_display_mode_t;


#define N_IRQ_TSTATES          32
#define N_DISPLAY_TIMINGS      (E_MACHINE_TYPE_LAST - E_MACHINE_TYPE_FIRST + 1)
#define N_REFRESH_FREQUENCIES  2


/**
 * https://wiki.specnext.dev/Refresh_Rates
 * https://wiki.specnext.dev/Reference_machines.
 *
 * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/blob/master/cores/zxnext/src/video/zxula_timing.vhd
 * (commit 2f13cb6e573af3d14dbd1c4b5918516af5c7c66f)
 *
 * Note that in the VHDL code above, pixel (0, 0) is the first pixel of the
 * 256x192 content area, i.e. the top and left borders are not included. The
 * tables below have been adjusted for this, such that (0, 0) is the first
 * pixel of the generated display, i.e. borders included.
 */
typedef struct {
  unsigned int rows;                   /** Number of rows.                  */
  unsigned int columns;                /** Number of columns.               */
  unsigned int hblank_start;           /** Column where HBLANK starts.      */
  unsigned int hblank_end;             /** Column where HBLANK ends.        */
  unsigned int vblank_start;           /** Row where VBLANK starts.         */
  unsigned int vblank_end;             /** Row where VBLANK ends.           */
  unsigned int vsync_row;              /** Row where VSYNC IRQ triggers.    */
  unsigned int vsync_column;           /** Column where VSYNC IRQ triggers. */
} ula_display_spec_t;


const ula_display_spec_t ula_display_spec_hdmi[N_REFRESH_FREQUENCIES]= {
  /**
   * HDMI, 50 Hz:                                        60 Hz:
   *
   *     0                256      323      395      432     0                256      323      392      429
   *   0 +----------------+--------+--------+--------+     0 +----------------+--------+--------+--------+
   *     | 192  content   | border | hblank | border |       | 192  content   | border | hblank | border |
   *     |        256     |   67   |   72   |   37   |       |        256     |   67   |   69   |   37   |
   * 192 +----------------+                          |   192 +----------------+                          |
   *     |  63  border                               |       |  38  border                               |
   * 255 +-----------------                          |   230 X-----------------                          |
   *     | X 9  vblank        X = (256,4)            |       | X 9  vblank         X = (235,4)           |
   * 264 +-----------------                          |   239 +-----------------                          |
   *     |  48  border                               |       |  23  border                               |
   * 312 +-------------------------------------------+   262 +-------------------------------------------+
   *
   * Also note that the display is generated in "half pixel" units (see
   * FRAME_BUFFER_{WIDTH,HEIGHT}), hence the macro.
   */

    /* 50 Hz */
    {
      .rows         = 312,
      .columns      = 432                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 67 + 72 + 5) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 63 + 9 + 16,
      .vsync_row    = 256,
      .vsync_column = 4                   * 2
    },
    /* 60 Hz */
    {
      .rows         = 262,
      .columns      = 429                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 67 + 69 + 5) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 38,  /* TODO Deal with HDMI's small lower border. */
      .vsync_row    = 235,
      .vsync_column = 4
    }
};


const ula_display_spec_t ula_display_spec_vga[N_DISPLAY_TIMINGS][N_REFRESH_FREQUENCIES] = {  
  /**
   * Note:
   * - X marks the spot of the vertical blank interrupt.
   * - Since the normal ULA display is measured in normal pixels and the
   *   frame buffer is generated in "half pixel" (horizontally), we have
   *   to multiply the columns by two.
   *
   * Configuration mode, like ZX 48K further down:
   */
  {
    /* 50 Hz */
    {
      .rows         = 312,
      .columns      = 448             * 2,
      .hblank_start = (256 + 32)      * 2,
      .hblank_end   = (256 + 64 + 96) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 56 + 8 + 24,
      .vsync_row    = 192 + 56,
      .vsync_column = 0
    },
    /* 60 Hz */
    {
      .rows         = 264,
      .columns      = 448             * 2,
      .hblank_start = (256 + 32)      * 2,
      .hblank_end   = (256 + 64 + 96) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 32 + 8,
      .vsync_row    = 192 + 32,
      .vsync_column = 0
    }
  },

  /**
   * VGA, ZX 48K, 50 Hz:                                 60 Hz:
   *
   *     0                256      320      416      448     0                256      320      416      448
   *   0 +----------------+--------+--------+--------+     0 +----------------+--------+--------+--------+
   *     | 192  content   | border | hblank | border |       | 192  content   | border | hblank | border |
   *     |        256     |   64   |   96   |   32   |       |        256     |   64   |   96   |   32   |
   * 192 +----------------+                          |   192 +----------------+                          |
   *     |  56  border                               |       |  32  border                               |
   * 248 X-----------------                          |   224 X-----------------                          |
   *     |   8  vblank                               |       |   8  vblank                               |
   * 256 +-----------------                          |   232 +-----------------                          |
   *     |  56  border                               |       |  32  border                               |
   * 312 +-------------------------------------------+   264 +-------------------------------------------+
   *
   * Also note that the display is generated in "half pixel" units (see
   * FRAME_BUFFER_{WIDTH,HEIGHT}), hence the macro.
   */
  {
    /* 50 Hz */
    {
      .rows         = 312,
      .columns      = 448             * 2,
      .hblank_start = (256 + 32)      * 2,
      .hblank_end   = (256 + 64 + 96) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 56 + 8 + 24,
      .vsync_row    = 192 + 56,
      .vsync_column = 0 
    },
    /* 60 Hz */
    {
      .rows         = 264,
      .columns      = 448             * 2,
      .hblank_start = (256 + 32)      * 2,
      .hblank_end   = (256 + 64 + 96) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 32 + 8,
      .vsync_row    = 192 + 32,
      .vsync_column = 0
    },
  },

  /**
   * VGA, ZX 128K, 50 Hz:                                60 Hz:
   *
   *     0                256      320      416      456     0                256      320      416      456
   *   0 +----------------+--------+--------+--------+     0 +----------------+--------+--------+--------+
   *     | 192  content   | border | hblank | border |       | 192  content   | border | hblank | border |
   *     |        256     |   64   |   96   |   40   |       |        256     |   64   |   96   |   40   |
   * 192 +----------------+                          |   192 +----------------+                          |
   *     |  56  border                               |       |  32  border                               |
   * 248 +---X------------- (at horizontal pos 4)    |   224 +---X------------- (at horizontal pos 4)    |
   *     |   8  vblank                               |       |   8  vblank                               |
   * 256 +-----------------                          |   232 +-----------------                          |
   *     |  55  border                               |       |  32  border                               |
   * 311 +-------------------------------------------+   264 +-------------------------------------------+
   */
  {
    /* 50 Hz */
    {
      .rows         = 311,
      .columns      = 456                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 64 + 96 + 8) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 56 + 8 + 23,
      .vsync_row    = 192 + 56,
      .vsync_column = 4                   * 2
    },
    /* 60 Hz */
    {
      .rows         = 264,
      .columns      = 456                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 64 + 96 + 8) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 32 + 8,
      .vsync_row    = 192 + 32,
      .vsync_column = 4                   * 2
    }
  },

  /**
   * VGA, +3, 50 Hz:                                     60 Hz:
   *
   *     0                256      320      416      456     0                256      320      416      456
   *   0 +----------------+--------+--------+--------+     0 +----------------+--------+--------+--------+
   *     | 192  content   | border | hblank | border |       | 192  content   | border | hblank | border |
   *     |        256     |   64   |   96   |   40   |       |        256     |   64   |   96   |   40   |
   * 192 +----------------+                          |   192 +----------------+                          |
   *     |  56  border                               |       |  32  border                               |
   * 248 +-X--------------- (at horizontal pos 2)    |   224 +-X--------------- (at horizontal pos 2)    |
   *     |   8  vblank                               |       |   8  vblank                               |
   * 256 +-----------------                          |   256 +-----------------                          |
   *     |  55  border                               |       |  32  border                               |
   * 311 +-------------------------------------------+   264 +-------------------------------------------+
   */
  {
    /* 50 Hz */
    {
      .rows         = 311,
      .columns      = 456                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 64 + 96 + 8) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 56 + 8 + 23,
      .vsync_row    = 192 + 56,
      .vsync_column = 2                   * 2
    },
    /* 60 Hz */
    {
      .rows         = 264,
      .columns      = 456                 * 2,
      .hblank_start = (256 + 32)          * 2,
      .hblank_end   = (256 + 64 + 96 + 8) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 32 + 8,
      .vsync_row    = 192 + 32,
      .vsync_column = 2                   * 2
    }
  },

  /**
   *
   * VGA, Pentagon, 50 Hz only:
   *
   *     0                256      336      400      448
   *   0 +----------------+--------+--------+--------+
   *     | 192  content   | border | hblank | border |
   *     |        256     |   80   |   64   |   48   |
   * 192 +----------------+                          |
   *     |  48  border                               |
   * 240 +-----------------      X (y=239 x=323)     |
   *     |  16  vblank                               |
   * 256 +-----------------                          |
   *     |  64  border                               |
   * 320 +-------------------------------------------+
   */
  {
    /* 50 Hz */
    {
      .rows         = 320,
      .columns      = 448                  * 2,
      .hblank_start = (256 + 32)           * 2,
      .hblank_end   = (256 + 80 + 64 + 16) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 48 + 16 + 32,
      .vsync_row    = 239,
      .vsync_column = 323                  * 2
    },
    /* 60 Hz = 50 Hz */
    {
      .rows         = 320,
      .columns      = 448                  * 2,
      .hblank_start = (256 + 32)           * 2,
      .hblank_end   = (256 + 80 + 64 + 16) * 2,
      .vblank_start = 192 + 32,
      .vblank_end   = 192 + 48 + 16 + 32,
      .vsync_row    = 239,
      .vsync_column = 323                  * 2
    }
  }    
};


typedef struct {
  u8_t*                      sram;
  const ula_display_spec_t*  display_spec;
  int                        did_display_spec_change;
  u8_t*                      display_ram;
  u8_t*                      display_ram_alt;
  machine_type_t             display_timing;
  ula_display_mode_t         display_mode;
  ula_display_mode_t         display_mode_requested;
  u8_t*                      attribute_ram;
  u8_t                       attribute_byte;
  u8_t                       border_colour;
  u8_t                       speaker_state;
  palette_t                  palette;
  int                        clip_x1;
  int                        clip_x2;
  int                        clip_y1;
  int                        clip_y2;
  uint64_t                   frame_counter;
  int                        blink_state;
  s8_t                       audio_last_sample;
  ula_screen_bank_t          screen_bank;
  int                        disable_ula_irq;
  u8_t                       hi_res_ink_colour;
  int                        do_contend;
  u32_t                      tstates_x4;
  int                        is_timex_enabled;
  int                        is_displaying_content;
  int                        is_enabled;
  u16_t                      transparency_rgb8;
  u8_t                       ula_next_mask_ink;
  u8_t                       ula_next_rshift_paper;
  int                        is_ula_next_mode;
  int                        is_60hz;
  int                        is_60hz_requested;
  int                        is_lo_res_enabled_requested;
  u8_t                       lo_res_offset_x;
  u8_t                       lo_res_offset_y;
  int                        is_hdmi;
  int                        is_hdmi_requested;
  u8_t                       offset_x;
  u8_t                       offset_y;
} self_t;


static self_t self;


#define N_DISPLAY_MODES   (E_ULA_DISPLAY_MODE_LAST - E_ULA_DISPLAY_MODE_FIRST + 1)


static const palette_entry_t* ula_display_mode_lo_res(u32_t row, u32_t column) {
  column = (self.lo_res_offset_x + column) % 256;
  row    = (self.lo_res_offset_y + row   ) % 192;

  column /= 2;
  row    /= 2;

  return palette_read(self.palette,
                      (row < 48)
                      ? self.display_ram[row * 128 + column]
                      : self.display_ram_alt[(row - 48) * 128 + column]);
}


static const palette_entry_t* ula_display_mode_hi_res(u32_t row, u32_t column) {
  const u8_t  mask             = 1 << (7 - (column & 0x07));
  const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | (((column >> 1) / 8) & 0x1F);;
  const u8_t* display_ram      = ((column / 8) & 0x01) ? self.display_ram_alt : self.display_ram;
  const u8_t  display_byte     = display_ram[display_offset];

  return palette_read(self.palette,
                      (display_byte & mask)
                      ? 0  + 8 + self.hi_res_ink_colour
                      : 16 + 8 + (~self.hi_res_ink_colour & 0x07));
}


static const palette_entry_t* ula_display_mode_screen_x(u32_t row, u32_t column) {
  const u32_t halved_column    = column / 2;
  const u16_t attribute_offset = (row / 8) * 32 + halved_column / 8;
  const u8_t  attribute_byte   = self.attribute_ram[attribute_offset];
  const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((halved_column / 8) & 0x1F);
  const u8_t  display_byte     = self.display_ram[display_offset];
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const int   is_foreground    = display_byte & mask;

  /* For floating-bus support. */
  self.attribute_byte = attribute_byte;

  if (self.is_ula_next_mode) {
    if (is_foreground) {
      return palette_read(self.palette, attribute_byte & self.ula_next_mask_ink);
    }
    
    if (self.ula_next_rshift_paper == 0) {
      return slu_transparent_get();
    }

    return palette_read(self.palette, 128 + ((attribute_byte & ~self.ula_next_mask_ink) >> self.ula_next_rshift_paper));
  }

  const u8_t             bright = (attribute_byte & 0x40) >> 3;
  const palette_entry_t* ink    = palette_read(self.palette, 0  + bright + (attribute_byte & 0x07));
  const palette_entry_t* paper  = palette_read(self.palette, 16 + bright + ((attribute_byte >> 3) & 0x07));;
  const u8_t             blink  = attribute_byte & 0x80;

  return is_foreground
    ? ((blink && self.blink_state) ? paper : ink)
    : ((blink && self.blink_state) ? ink : paper);
}

static const palette_entry_t* ula_display_mode_hi_colour(u32_t row, u32_t column) {
  const u32_t halved_column    = column / 2;
  const u16_t attribute_offset = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((halved_column / 8) & 0x1F);
  const u8_t  attribute_byte   = self.attribute_ram[attribute_offset];
  const u8_t  display_byte     = self.display_ram[attribute_offset];
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const int   is_foreground    = display_byte & mask;

  /* For floating-bus support. */
  self.attribute_byte = attribute_byte;

  if (self.is_ula_next_mode) {
    if (is_foreground) {
      return palette_read(self.palette, attribute_byte & self.ula_next_mask_ink);
    }
    
    if (self.ula_next_rshift_paper == 0) {
      return slu_transparent_get();
    }

    return palette_read(self.palette, 128 + ((attribute_byte & ~self.ula_next_mask_ink) >> self.ula_next_rshift_paper));
  }

  const u8_t             bright = (attribute_byte & 0x40) >> 3;
  const palette_entry_t* ink    = palette_read(self.palette, 0  + bright + (attribute_byte & 0x07));
  const palette_entry_t* paper  = palette_read(self.palette, 16 + bright + ((attribute_byte >> 3) & 0x07));;
  const u8_t             blink  = attribute_byte & 0x80;

  return is_foreground
    ? ((blink && self.blink_state) ? paper : ink)
    : ((blink && self.blink_state) ? ink : paper);
}


typedef const palette_entry_t* (*ula_display_mode_handler_t)(u32_t beam_row, u32_t beam_column);

const ula_display_mode_handler_t ula_display_handlers[N_DISPLAY_MODES] = {  
  ula_display_mode_screen_x,   /* E_ULA_DISPLAY_MODE_SCREEN_0 */
  ula_display_mode_screen_x,   /* E_ULA_DISPLAY_MODE_SCREEN_1 */
  ula_display_mode_hi_colour,  /* E_ULA_DISPLAY_MODE_HI_COLOUR */
  ula_display_mode_hi_res,     /* E_ULA_DISPLAY_MODE_HI_RES */
  ula_display_mode_lo_res      /* E_ULA_DISPLAY_MODE_LO_RES */
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
  const ula_display_mode_t mode = \
    self.is_lo_res_enabled_requested          ? E_ULA_DISPLAY_MODE_LO_RES :
    (self.screen_bank == E_ULA_SCREEN_BANK_7) ? (self.display_mode_requested & 1) :
    /* else */                                  self.display_mode_requested;

  /* Remember the requested mode in case we honor it in bank 5. */
  self.display_mode = mode;
  self.is_60hz      = self.is_60hz_requested;
  self.is_hdmi      = self.is_hdmi_requested;
  self.display_spec = self.is_hdmi
    ? &ula_display_spec_hdmi[self.is_60hz & 1]
    : &ula_display_spec_vga[self.display_timing][self.is_60hz & 1];

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
      self.display_ram_alt = &self.display_ram[0x2000];
      break;

    case E_ULA_DISPLAY_MODE_LO_RES:
      self.display_ram     = &self.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];
      self.display_ram_alt = &self.display_ram[0x2000];
      break;
  }

  slu_display_size_set(self.display_spec->rows, self.display_spec->columns);

  main_show_refresh(self.is_60hz);
}


void ula_did_complete_frame(void) {
  /**
   * https://worldofspectrum.org/faq/reference/48kreference.htm#Hardware
   *
   * > The Spectrum's 'FLASH' effect is also produced by the ULA: Every 16
   * > frames, the ink and paper of all flashing bytes is swapped; ie a normal
   * > to inverted to normal cycle takes 32 frames, which is (good as) 0.64
   * > seconds.
   */
  if ((++self.frame_counter & 15) == 0) {
    self.blink_state ^= 1;
  }

  if (self.did_display_spec_change) {
    ula_display_reconfigure();
    self.did_display_spec_change = 0;
  }
}


/**
 * Given the CRT's beam position, where (0, 0) is the first pixel of the
 * (typically) 256x192 content area, returns a flag indicating whether it falls
 * within the visible 320x256 frame buffer (in truth it's 640x256).
 *
 * Only if it returns non-zero does it also return the frame buffer position,
 * (0, 0) is the first pixel of the border, such that we can align the other
 * layers which use the 320x256 or 640x256 resolutions.
 */
int ula_beam_to_frame_buffer(u32_t beam_row, u32_t beam_column, u32_t* frame_buffer_row, u32_t* frame_buffer_column) {
  /**
   * tstates are expressed in the stock 3.5 MHz clock, but this function is
   * called at a 14 MHz rate, hence the "times four".
   */
  self.tstates_x4++;

  if (beam_row == self.display_spec->vsync_row && beam_column == self.display_spec->vsync_column) {
    copper_irq();
    if (!self.disable_ula_irq) {
      cpu_irq();
    }
    self.tstates_x4 = 0;
  } else if (self.tstates_x4 / 4 < N_IRQ_TSTATES && !self.disable_ula_irq) {
    cpu_irq();
  }

  /* Need to know this for floating bus support. */
  self.is_displaying_content = beam_row < 192 && beam_column / 2 < 256;

  /* Nothing to draw when beam is outside visible area. */
  if (beam_row >= self.display_spec->vblank_start && beam_row < self.display_spec->vblank_end) {
    /* In VBLANK. */
    return 0;
  }
  if (beam_column >= self.display_spec->hblank_start && beam_column < self.display_spec->hblank_end) {
    /* In HBLANK. */
    return 0;
  }

  /* Convert beam position to frame buffer position, where (0, 0) is first
   * visible border pixel. */
  *frame_buffer_row    = (beam_row    >= self.display_spec->vblank_end) ? (beam_row    - self.display_spec->vblank_end) : (beam_row    + 32);
  *frame_buffer_column = (beam_column >= self.display_spec->hblank_end) ? (beam_column - self.display_spec->hblank_end) : (beam_column + 32 * 2);

  return 1;
}


/**
 * Returns the ULA pixel colour for the frame buffer position, if any, and
 * whether it is transparent or not.
 */
void ula_tick(u32_t row, u32_t column, int* is_enabled, int* is_border, int* is_clipped, const palette_entry_t** rgb) {
  *is_enabled = self.is_enabled;
  if (!self.is_enabled) {
    return;
  }

  if (self.is_displaying_content) {
    row    -= 32;
    column -= 32 * 2;

    *is_border  = 0;
    *is_clipped = (row        < self.clip_y1 || row        > self.clip_y2 ||
                   column / 2 < self.clip_x1 || column / 2 > self.clip_x2);

    row     = (row    + self.offset_y    ) % 192;
    column  = (column + self.offset_x * 2) % (256 * 2);
 
    *rgb = ula_display_handlers[self.display_mode](row, column);
    return;
  }
  
  *is_border  = 1;
  *is_clipped = 0;

  if (self.is_ula_next_mode) {
    if (self.ula_next_rshift_paper == 0) {
      *rgb = slu_transparent_get();
    } else {
      *rgb = palette_read(self.palette, 128 + self.border_colour);
    }
  } else {
    *rgb = palette_read(self.palette, 16 + self.border_colour);
  }
}


int ula_init(u8_t* sram) {
  self.sram                = sram;
  self.speaker_state       = 0;
  self.audio_last_sample   = 0;
  self.display_timing      = E_MACHINE_TYPE_ZX_48K;
  self.is_60hz             = 0;
  self.is_hdmi             = 0;

  ula_reset(E_RESET_HARD);

  return 0;
}


void ula_finit(void) {
}


void ula_reset(reset_t reset) {
  self.clip_x1               = 0;
  self.clip_x2               = 255;
  self.clip_y1               = 0;
  self.clip_y2               = 191;
  self.disable_ula_irq       = 0;
  self.palette               = E_PALETTE_ULA_FIRST;
  self.is_ula_next_mode      = 0;
  self.ula_next_mask_ink     = 7;
  self.ula_next_rshift_paper = 3;
  self.is_enabled            = 1;
  self.border_colour         = 0;
  self.hi_res_ink_colour     = 0;
  self.screen_bank           = E_ULA_SCREEN_BANK_5;
  self.do_contend            = 1;
  self.offset_x              = 0;
  self.offset_y              = 0;

  if (reset == E_RESET_HARD) {
    self.is_timex_enabled = 0;
  }

  self.display_mode_requested      = E_ULA_DISPLAY_MODE_SCREEN_0;
  self.is_lo_res_enabled_requested = 0;
  self.did_display_spec_change     = 1;
  ula_display_reconfigure();
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
}


u8_t ula_timex_read(u16_t address) {
  if (!self.is_timex_enabled) {
    return ula_floating_bus_read();
  }
  
  return self.disable_ula_irq   << 6
         | self.hi_res_ink_colour << 3
         | self.display_mode;
}


void ula_timex_write(u16_t address, u8_t value) {
  self.disable_ula_irq   = (value & 0x40) >> 6;
  self.hi_res_ink_colour = (value & 0x38) >> 3;

  switch (value & 0x07) {
    case 0x00:
      self.display_mode_requested  = E_ULA_DISPLAY_MODE_SCREEN_0;
      self.did_display_spec_change = 1;
      break;

    case 0x01:
      self.display_mode_requested  = E_ULA_DISPLAY_MODE_SCREEN_1;
      self.did_display_spec_change = 1;
      break;

    case 0x02:
      self.display_mode_requested  = E_ULA_DISPLAY_MODE_HI_COLOUR;
      self.did_display_spec_change = 1;
      break;

    case 0x06:
      self.display_mode_requested  = E_ULA_DISPLAY_MODE_HI_RES;
      self.did_display_spec_change = 1;
      break;

    default:
      break;
  }
}


machine_type_t ula_timing_get(void) {
  return self.display_timing;
}


void ula_timing_set(machine_type_t machine) {
  if (machine < E_MACHINE_TYPE_ZX_48K || machine > E_MACHINE_TYPE_LAST) {
    log_err("ula: refused to set timing to %d\n", machine);
    return;
  }

  self.display_timing          = machine;
  self.did_display_spec_change = 1;

  main_show_machine_type(self.display_timing);
}


void ula_palette_set(int use_second) {
  self.palette = use_second ? E_PALETTE_ULA_SECOND : E_PALETTE_ULA_FIRST;
}


void ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;
}


void ula_screen_bank_set(ula_screen_bank_t bank) {
  if (bank != self.screen_bank) {
    self.screen_bank            = bank;
    self.display_mode_requested = self.display_mode;  /* Does not change. */

    /* Changing the bank has an immediate effect. */
    ula_display_reconfigure();
  }
}


ula_screen_bank_t ula_screen_bank_get(void) {
  return self.screen_bank;
}


int ula_contention_get(void) {
  return self.do_contend;
}


void ula_contention_set(int do_contend) {
  self.do_contend = do_contend;
}


/**
 * https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
 *
 * The contention actually starts at t-state 14335, i.e. one t-state *before*
 * the first pixel in the top-left corner is being drawn.
 *
 * We could otherwise have used self.displaying_content, but that is one t-state
 * too late.
 */
static void ula_contend_48k(void) {
  const u32_t delays[8] = {
    6, 5, 4, 3, 2, 1, 0, 0
  };
  const u32_t tstates = self.tstates_x4 / 4;
  u32_t       relative_tstate;

  if (tstates < 14335 || tstates >= 14335 + 192 * 224) {
    /* Outside visible area. */
    return;
  }

  relative_tstate = (tstates - 14335) % 224;
  if (relative_tstate >= 128) {
    /* In one of the borders or horizontal blanking. */
    return;
  }

  clock_run(delays[relative_tstate % 8]);
}


static void ula_contend_128k(void) {
  if (self.is_displaying_content) {
    const u32_t delay[8] = {
      6, 5, 4, 3, 2, 1, 0, 0
    };
    const u32_t t_states = self.tstates_x4 / 4;

    clock_run(delay[((t_states + 1) % 228) % 8]);
  }
}


static void ula_contend_plus_2(void) {
}


void ula_contend(u8_t bank) {
  if (!self.do_contend) {
    /* Contention can be disabled by writing to a Next register. */
    return;
  }

  if (clock_cpu_speed_get() != E_CPU_SPEED_3MHZ) {
    /* Contention only plays a role when running at 3.5 MHz. */
    return;
  }

  if (bank > 7) {
    /* Only banks 0-7 can be contended. */
    return;
  }

  switch (self.display_timing) {
    case E_MACHINE_TYPE_ZX_48K:
      if (bank == 5) {
        /* Only bank 5 is contended. */
        ula_contend_48k();
      }
      break;

    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      if ((bank & 1) == 1) {
        /* Only odd banks are contended. */
        ula_contend_128k();
      }
      break;

    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      if (bank > 3) {
        /* Only banks four and above are contended. */
        ula_contend_plus_2();
      }
      break;

    default:
      break;
  }
}


void ula_enable_set(int enable) {
  self.is_enabled = enable;
}


void ula_transparency_colour_write(u8_t rgb) {
  self.transparency_rgb8 = rgb;
}


void ula_attribute_byte_format_write(u8_t value) {
  self.ula_next_mask_ink = value;;

  switch (value) {
    case 1:
      self.ula_next_rshift_paper = 1;
      break;
 
    case 3:
      self.ula_next_rshift_paper = 2;
      break;
 
    case 7:
      self.ula_next_rshift_paper = 3;
      break;
 
    case 15:
      self.ula_next_rshift_paper = 4;
      break;
 
    case 31:
      self.ula_next_rshift_paper = 5;
      break;
 
    case 63:
      self.ula_next_rshift_paper = 6;
      break;
 
    case 127:
      self.ula_next_rshift_paper = 7;
      break;

    default:
      self.ula_next_rshift_paper = 0;
      break;      
  }
}


u8_t ula_attribute_byte_format_read(void) {
  return self.ula_next_mask_ink;
}


void ula_next_mode_enable(int do_enable) {
  self.is_ula_next_mode = do_enable;
}


void ula_60hz_set(int enable) {
  self.is_60hz_requested       = enable;
  self.did_display_spec_change = 1;
}


int ula_60hz_get(void) {
  return self.is_60hz;
}


int ula_irq_enable_get(void) {
  return !self.disable_ula_irq;
}


void ula_irq_enable_set(int enable) {
  self.disable_ula_irq = !enable;
}


void ula_lo_res_enable_set(int enable) {
  self.is_lo_res_enabled_requested = enable;
  self.did_display_spec_change     = 1;
}


void ula_lo_res_offset_x_write(u8_t value) {
  self.lo_res_offset_x = value;
}


void ula_lo_res_offset_y_write(u8_t value) {
  self.lo_res_offset_y = value;
}


void ula_hdmi_enable(int enable) {
  self.is_hdmi_requested       = enable;
  self.did_display_spec_change = 1;
}


u8_t ula_offset_x_read(void) {
  return self.offset_x;
}


void ula_offset_x_write(u8_t value) {
  self.offset_x = value;
}


u8_t ula_offset_y_read(void) {
  return self.offset_y;
}


void ula_offset_y_write(u8_t value) {
  self.offset_y = value;
}


/**
 * Implement floating bus behaviour. This fixes Arkanoid freezing at the
 * start of the first level and prevents flickering and slowdown in
 * Short Circuit.
 */
u8_t ula_floating_bus_read(void) {
  return self.is_displaying_content
    ? self.attribute_byte
    : 0xFF;
}
