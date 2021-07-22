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


typedef enum ula_display_mode_t {
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
typedef struct ula_display_spec_t {
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


typedef struct ula_t {
  u8_t*                      sram;
  const ula_display_spec_t*  display_spec;
  int                        did_display_spec_change;
  u8_t*                      display_ram;
  u8_t*                      display_ram_alt;
  machine_type_t             display_timing;
  ula_display_mode_t         display_mode;
  ula_display_mode_t         display_mode_requested;
  u8_t*                      attribute_ram;
  u8_t                       border_colour_latched;
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
  int                        is_enabled;
  const palette_entry_t*     transparent;
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
} ula_t;


static ula_t ula;


#define N_DISPLAY_MODES   (E_ULA_DISPLAY_MODE_LAST - E_ULA_DISPLAY_MODE_FIRST + 1)


inline
static const palette_entry_t* ula_display_mode_lo_res(u32_t row, u32_t column) {
  column = (ula.lo_res_offset_x + column) % 256;
  row    = (ula.lo_res_offset_y + row   ) % 192;

  column /= 2;
  row    /= 2;

  return palette_read_inline(ula.palette,
                             (row < 48)
                             ? ula.display_ram[row * 128 + column]
                             : ula.display_ram_alt[(row - 48) * 128 + column]);
}


inline
static const palette_entry_t* ula_display_mode_hi_res(u32_t row, u32_t column) {
  const u8_t  mask             = 1 << (7 - (column & 0x07));
  const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | (((column >> 1) / 8) & 0x1F);;
  const u8_t* display_ram      = ((column / 8) & 0x01) ? ula.display_ram_alt : ula.display_ram;
  const u8_t  display_byte     = display_ram[display_offset];

  return palette_read_inline(ula.palette,
                      (display_byte & mask)
                      ? 0  + 8 + ula.hi_res_ink_colour
                      : 16 + 8 + (~ula.hi_res_ink_colour & 0x07));
}


inline
static const u8_t ula_mode_x_display_byte_get(u32_t row, u32_t column) {
  const u16_t display_offset = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((column / 8) & 0x1F);

  return ula.display_ram[display_offset];
}


inline
static const u8_t ula_mode_x_attribute_byte_get(u32_t row, u32_t column) {
  const u16_t attribute_offset = (row / 8) * 32 + column / 8;
  
  return ula.attribute_ram[attribute_offset];
}


inline
static const palette_entry_t* ula_display_mode_screen_x(u32_t row, u32_t column) {
  const u32_t halved_column    = column / 2;
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const u8_t  display_byte     = ula_mode_x_display_byte_get(row, halved_column);
  const u8_t  attribute_byte   = ula_mode_x_attribute_byte_get(row, halved_column);
  const int   is_foreground    = display_byte & mask;

  if (ula.is_ula_next_mode) {
    if (is_foreground) {
      return palette_read_inline(ula.palette, attribute_byte & ula.ula_next_mask_ink);
    }
    
    if (ula.ula_next_rshift_paper == 0) {
      return ula.transparent;
    }

    return palette_read_inline(ula.palette, 128 + ((attribute_byte & ~ula.ula_next_mask_ink) >> ula.ula_next_rshift_paper));
  }

  const u8_t             bright = (attribute_byte & 0x40) >> 3;
  const palette_entry_t* ink    = palette_read_inline(ula.palette, 0  + bright + (attribute_byte & 0x07));
  const palette_entry_t* paper  = palette_read_inline(ula.palette, 16 + bright + ((attribute_byte >> 3) & 0x07));;
  const u8_t             blink  = attribute_byte & 0x80;

  return is_foreground
    ? ((blink && ula.blink_state) ? paper : ink)
    : ((blink && ula.blink_state) ? ink : paper);
}


inline
static const palette_entry_t* ula_display_mode_hi_colour(u32_t row, u32_t column) {
  const u32_t halved_column    = column / 2;
  const u16_t attribute_offset = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((halved_column / 8) & 0x1F);
  const u8_t  attribute_byte   = ula.attribute_ram[attribute_offset];
  const u8_t  display_byte     = ula.display_ram[attribute_offset];
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const int   is_foreground    = display_byte & mask;

  if (ula.is_ula_next_mode) {
    if (is_foreground) {
      return palette_read_inline(ula.palette, attribute_byte & ula.ula_next_mask_ink);
    }
    
    if (ula.ula_next_rshift_paper == 0) {
      return ula.transparent;
    }

    return palette_read_inline(ula.palette, 128 + ((attribute_byte & ~ula.ula_next_mask_ink) >> ula.ula_next_rshift_paper));
  }

  const u8_t             bright = (attribute_byte & 0x40) >> 3;
  const palette_entry_t* ink    = palette_read_inline(ula.palette, 0  + bright + (attribute_byte & 0x07));
  const palette_entry_t* paper  = palette_read_inline(ula.palette, 16 + bright + ((attribute_byte >> 3) & 0x07));;
  const u8_t             blink  = attribute_byte & 0x80;

  return is_foreground
    ? ((blink && ula.blink_state) ? paper : ink)
    : ((blink && ula.blink_state) ? ink : paper);
}


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
    ula.is_lo_res_enabled_requested          ? E_ULA_DISPLAY_MODE_LO_RES :
    (ula.screen_bank == E_ULA_SCREEN_BANK_7) ? (ula.display_mode_requested & 1) :
    /* else */                                 ula.display_mode_requested;

  /* Remember the requested mode in case we honor it in bank 5. */
  ula.display_mode      = mode;
  ula.is_60hz           = ula.is_60hz_requested;
  ula.is_hdmi           = ula.is_hdmi_requested;
  ula.display_spec      = ula.is_hdmi
    ? &ula_display_spec_hdmi[ula.is_60hz & 1]
    : &ula_display_spec_vga[ula.display_timing][ula.is_60hz & 1];

  /* Use the effective mode, which depends on the actual bank. */
  switch (mode) {
    case E_ULA_DISPLAY_MODE_SCREEN_0:
      ula.display_ram   = &ula.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + ula.screen_bank * 16 * 1024];
      ula.attribute_ram = &ula.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_SCREEN_1:
      ula.display_ram   = &ula.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + ula.screen_bank * 16 * 1024 + 0x2000];
      ula.attribute_ram = &ula.display_ram[192 * 32];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_COLOUR:
      ula.display_ram   = &ula.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + ula.screen_bank * 16 * 1024];
      ula.attribute_ram = &ula.display_ram[0x2000];
      break;
      
    case E_ULA_DISPLAY_MODE_HI_RES:
      ula.display_ram     = &ula.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + ula.screen_bank * 16 * 1024];
      ula.display_ram_alt = &ula.display_ram[0x2000];
      break;

    case E_ULA_DISPLAY_MODE_LO_RES:
      ula.display_ram     = &ula.sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM + 5 * 16 * 1024];
      ula.display_ram_alt = &ula.display_ram[0x2000];
      break;
  }

  slu_display_size_set(ula.display_spec->rows, ula.display_spec->columns);

  main_show_refresh(ula.is_60hz);
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
  if ((++ula.frame_counter & 15) == 0) {
    ula.blink_state ^= 1;
  }

  if (ula.did_display_spec_change) {
    ula_display_reconfigure();
    ula.did_display_spec_change = 0;
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
inline
static int ula_beam_to_frame_buffer(u32_t beam_row, u32_t beam_column, u32_t* frame_buffer_row, u32_t* frame_buffer_column) {
  /**
   * tstates are expressed in the stock 3.5 MHz clock, but this function is
   * called at a 14 MHz rate, hence the "times four".
   */
  ula.tstates_x4++;

  if (beam_row == ula.display_spec->vsync_row && beam_column == ula.display_spec->vsync_column) {
    copper_irq();
    if (!ula.disable_ula_irq) {
      cpu_irq(E_CPU_IRQ_ULA, 1);
    }
    ula.tstates_x4 = 0;
  } else if (ula.tstates_x4 == N_IRQ_TSTATES * 4) {
    cpu_irq(E_CPU_IRQ_ULA, 0);
  }

  /* Nothing to draw when beam is outside visible area. */
  if (beam_row >= ula.display_spec->vblank_start && beam_row < ula.display_spec->vblank_end) {
    /* In VBLANK. */
    return 0;
  }
  if (beam_column >= ula.display_spec->hblank_start && beam_column < ula.display_spec->hblank_end) {
    /* In HBLANK. */
    return 0;
  }

  /* Convert beam position to frame buffer position, where (0, 0) is first
   * visible border pixel. */
  *frame_buffer_row    = (beam_row    >= ula.display_spec->vblank_end) ? (beam_row    - ula.display_spec->vblank_end) : (beam_row    + 32);
  *frame_buffer_column = (beam_column >= ula.display_spec->hblank_end) ? (beam_column - ula.display_spec->hblank_end) : (beam_column + 32 * 2);

  return 1;
}


/**
 * Returns the ULA pixel colour for the frame buffer position, if any, and
 * whether it is transparent or not.
 */
inline
static void ula_tick(u32_t row, u32_t column, int* is_enabled, int* is_border, int* is_clipped, const palette_entry_t** rgb) {
  *is_enabled = ula.is_enabled;
  if (!ula.is_enabled) {
    return;
  }

  /* Latch border colour every 4th T-state. */
  if (ula.tstates_x4 % (4 * 4) == 0) {
    ula.border_colour = ula.border_colour_latched;
  }

  if (row >= 32 && row < 32 + 192 && column >= 32 * 2 && column < (32 + 256) * 2) {
    row    -= 32;
    column -= 32 * 2;

    *is_border  = 0;
    *is_clipped = (row        < ula.clip_y1 || row        > ula.clip_y2 ||
                   column / 2 < ula.clip_x1 || column / 2 > ula.clip_x2);

    row     = (row    + ula.offset_y    ) % 192;
    column  = (column + ula.offset_x * 2) % (256 * 2);
 
    /* *rgb = ula_display_handlers[ula.display_mode](row, column); */
    switch (ula.display_mode) {
      case E_ULA_DISPLAY_MODE_HI_COLOUR:
        *rgb = ula_display_mode_hi_colour(row, column);
        break;
      
      case E_ULA_DISPLAY_MODE_HI_RES:
        *rgb = ula_display_mode_hi_res(row, column);
        break;
      
      case E_ULA_DISPLAY_MODE_LO_RES:
        *rgb = ula_display_mode_lo_res(row, column);
        break;

      default:
        *rgb = ula_display_mode_screen_x(row, column);
        break;
    }

    return;
  }

  *is_border  = 1;
  *is_clipped = 0;

  if (ula.is_ula_next_mode) {
    if (ula.ula_next_rshift_paper == 0) {
      *rgb = ula.transparent;
    } else {
      *rgb = palette_read_inline(ula.palette, 128 + ula.border_colour);
    }
  } else {
    *rgb = palette_read_inline(ula.palette, 16 + ula.border_colour);
  }
}


int ula_init(u8_t* sram) {
  ula.sram                = sram;
  ula.speaker_state       = 0;
  ula.audio_last_sample   = 0;
  ula.display_timing      = E_MACHINE_TYPE_ZX_48K;
  ula.is_60hz             = 0;
  ula.is_hdmi             = 0;

  ula_reset(E_RESET_HARD);

  return 0;
}


void ula_finit(void) {
}


void ula_reset(reset_t reset) {
  ula.clip_x1               = 0;
  ula.clip_x2               = 255;
  ula.clip_y1               = 0;
  ula.clip_y2               = 191;
  ula.disable_ula_irq       = 0;
  ula.palette               = E_PALETTE_ULA_FIRST;
  ula.is_ula_next_mode      = 0;
  ula.ula_next_mask_ink     = 7;
  ula.ula_next_rshift_paper = 3;
  ula.is_enabled            = 1;
  ula.border_colour         = 0;
  ula.hi_res_ink_colour     = 0;
  ula.screen_bank           = E_ULA_SCREEN_BANK_5;
  ula.do_contend            = 1;
  ula.offset_x              = 0;
  ula.offset_y              = 0;

  if (reset == E_RESET_HARD) {
    ula.is_timex_enabled = 0;
  }

  ula.display_mode_requested      = E_ULA_DISPLAY_MODE_SCREEN_0;
  ula.is_lo_res_enabled_requested = 0;
  ula.did_display_spec_change     = 1;
  ula_display_reconfigure();
}


u8_t ula_read(u16_t address) {
  return keyboard_read(address);
}


void ula_write(u16_t address, u8_t value) {
  const u8_t speaker_state = value & 0x10;
  const s8_t sample        = speaker_state ? AUDIO_MAX_VOLUME : 0;

  audio_add_sample(E_AUDIO_SOURCE_BEEPER, sample);

  ula.speaker_state         = speaker_state;
  ula.border_colour_latched = value & 0x07;
}


void ula_timex_video_mode_read_enable(int do_enable) {
  ula.is_timex_enabled = do_enable;
}


u8_t ula_timex_read(u16_t address) {
  if (!ula.is_timex_enabled) {
    return ula_floating_bus_read();
  }
  
  return ula.disable_ula_irq   << 6
         | ula.hi_res_ink_colour << 3
         | ula.display_mode;
}


void ula_timex_write(u16_t address, u8_t value) {
  ula.disable_ula_irq = (value & 0x40) >> 6;
  if (ula.disable_ula_irq) {
    cpu_irq(E_CPU_IRQ_ULA, 0);
  }

  ula.hi_res_ink_colour = (value & 0x38) >> 3;

  switch (value & 0x07) {
    case 0x00:
      ula.display_mode_requested  = E_ULA_DISPLAY_MODE_SCREEN_0;
      ula.did_display_spec_change = 1;
      break;

    case 0x01:
      ula.display_mode_requested  = E_ULA_DISPLAY_MODE_SCREEN_1;
      ula.did_display_spec_change = 1;
      break;

    case 0x02:
      ula.display_mode_requested  = E_ULA_DISPLAY_MODE_HI_COLOUR;
      ula.did_display_spec_change = 1;
      break;

    case 0x06:
      ula.display_mode_requested  = E_ULA_DISPLAY_MODE_HI_RES;
      ula.did_display_spec_change = 1;
      break;

    default:
      break;
  }
}


machine_type_t ula_timing_get(void) {
  return ula.display_timing;
}


void ula_timing_set(machine_type_t machine) {
  if (machine < E_MACHINE_TYPE_ZX_48K || machine > E_MACHINE_TYPE_LAST) {
    log_err("ula: refused to set timing to %d\n", machine);
    return;
  }

  ula.display_timing          = machine;
  ula.did_display_spec_change = 1;

  main_show_machine_type(ula.display_timing);
}


void ula_palette_set(int use_second) {
  ula.palette = use_second ? E_PALETTE_ULA_SECOND : E_PALETTE_ULA_FIRST;
}


void ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  ula.clip_x1 = x1;
  ula.clip_x2 = x2;
  ula.clip_y1 = y1;
  ula.clip_y2 = y2;
}


void ula_screen_bank_set(ula_screen_bank_t bank) {
  if (bank != ula.screen_bank) {
    ula.screen_bank            = bank;
    ula.display_mode_requested = ula.display_mode;  /* Does not change. */

    /* Changing the bank has an immediate effect. */
    ula_display_reconfigure();
  }
}


ula_screen_bank_t ula_screen_bank_get(void) {
  return ula.screen_bank;
}


int ula_contention_get(void) {
  return ula.do_contend;
}


void ula_contention_set(int do_contend) {
  ula.do_contend = do_contend;
}


/**
 * https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
 *
 * The contention actually starts at t-state 14335, i.e. one t-state *before*
 * the first pixel in the top-left corner is being drawn.
 *
 * We could otherwise have used ula.displaying_content, but that is one t-state
 * too late.
 */
static void ula_contend_48k(void) {
  const u32_t delays[8] = {
    6, 5, 4, 3, 2, 1, 0, 0
  };
  const u32_t tstates = ula.tstates_x4 / 4;
  u32_t       relative_tstate;
  u32_t       delay;

  if (tstates < 14335 || tstates >= 14335 + 192 * 224) {
    /* Outside visible area. */
    return;
  }

  relative_tstate = (tstates - 14335) % 224;
  if (relative_tstate >= 128) {
    /* In one of the borders or horizontal blanking. */
    return;
  }

  delay = delays[relative_tstate % 8];
  if (delay) {
    clock_run(delay);
  }
}


typedef void (*contend_handler)(void);


void ula_contend(void) {
  const contend_handler handlers[E_MACHINE_TYPE_LAST - E_MACHINE_TYPE_FIRST + 1] = {
    NULL,
    ula_contend_48k,
    NULL,
    NULL,
    NULL
  };

  contend_handler handler;

  if (!ula.do_contend) {
    /* Contention can be disabled by writing to a Next register. */
    return;
  }

  if (clock_cpu_speed_get() != E_CPU_SPEED_3MHZ) {
    /* Contention only plays a role when running at 3.5 MHz. */
    return;
  }

  handler = handlers[ula.display_timing];
  if (handler) {
    handler();
  }
}


void ula_contend_bank(u8_t bank) {
  if (bank > 7) {
    /* Only banks 0-7 can be contended. */
    return;
  }

  switch (ula.display_timing) {
    case E_MACHINE_TYPE_ZX_48K:
      if (bank == 5) {
        /* Only bank 5 is contended. */
        ula_contend();
      }
      break;

    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      if ((bank & 1) == 1) {
        /* Only odd banks are contended. */
        ula_contend();
      }
      break;

    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      if (bank > 3) {
        /* Only banks four and above are contended. */
        ula_contend();
      }
      break;

    default:
      break;
  }
}


void ula_enable_set(int enable) {
  ula.is_enabled = enable;
}


void ula_transparent_set(const palette_entry_t* transparent) {
  ula.transparent = transparent;
}


void ula_attribute_byte_format_write(u8_t value) {
  ula.ula_next_mask_ink = value;;

  switch (value) {
    case 1:
      ula.ula_next_rshift_paper = 1;
      break;
 
    case 3:
      ula.ula_next_rshift_paper = 2;
      break;
 
    case 7:
      ula.ula_next_rshift_paper = 3;
      break;
 
    case 15:
      ula.ula_next_rshift_paper = 4;
      break;
 
    case 31:
      ula.ula_next_rshift_paper = 5;
      break;
 
    case 63:
      ula.ula_next_rshift_paper = 6;
      break;
 
    case 127:
      ula.ula_next_rshift_paper = 7;
      break;

    default:
      ula.ula_next_rshift_paper = 0;
      break;      
  }
}


u8_t ula_attribute_byte_format_read(void) {
  return ula.ula_next_mask_ink;
}


void ula_next_mode_enable(int do_enable) {
  ula.is_ula_next_mode = do_enable;
}


void ula_60hz_set(int enable) {
  ula.is_60hz_requested       = enable;
  ula.did_display_spec_change = 1;
}


int ula_60hz_get(void) {
  return ula.is_60hz;
}


int ula_irq_enable_get(void) {
  return !ula.disable_ula_irq;
}


void ula_irq_enable_set(int enable) {
  ula.disable_ula_irq = !enable;
}


int ula_lo_res_enable_get(void) {
  return ula.is_lo_res_enabled_requested;
}


void ula_lo_res_enable_set(int enable) {
  ula.is_lo_res_enabled_requested = enable;
  ula.did_display_spec_change     = 1;
}


void ula_lo_res_offset_x_write(u8_t value) {
  ula.lo_res_offset_x = value;
}


void ula_lo_res_offset_y_write(u8_t value) {
  ula.lo_res_offset_y = value;
}


void ula_hdmi_enable(int enable) {
  ula.is_hdmi_requested       = enable;
  ula.did_display_spec_change = 1;
}


u8_t ula_offset_x_read(void) {
  return ula.offset_x;
}


void ula_offset_x_write(u8_t value) {
  ula.offset_x = value;
}


u8_t ula_offset_y_read(void) {
  return ula.offset_y;
}


void ula_offset_y_write(u8_t value) {
  ula.offset_y = value;
}


static u8_t ula_floating_bus_48k_read(void) {
  if (ula.tstates_x4 / 4 < 14338) {
    return 0xFF;
  }

  const u32_t tstates = ula.tstates_x4 / 4 - 14338;
  const u32_t row     =  tstates / 224;
  const u32_t col     = (tstates % 224) * 2;

  if (col >= 256) {
    /* Right border/horizontal flyback/left border. */
    return 0xFF;
  }

  switch (tstates % 8) {
    case 0:  /* 14338 + 8N */
      return ula_mode_x_display_byte_get(row, col);
    
    case 1:  /* 14339 + 8N */
      return ula_mode_x_attribute_byte_get(row, col);

    case 2:  /* 14340 + 8N */
      return ula_mode_x_display_byte_get(row, col + 8);
    
    case 3: /* 14341 + 8N */
      return ula_mode_x_attribute_byte_get(row, col + 8);

    default:
      break;
  }

  return 0xFF;
}


static u8_t ula_floating_bus_128k_read(void) {
  const u32_t tstates = ula.tstates_x4 / 4 - 14364;
  const u32_t row     =  tstates / 228;
  const u32_t col     = (tstates % 228) * 2;

  if (col >= 256) {
    /* Right border/horizontal flyback/left border. */
    return 0xFF;
  }

  switch (tstates % 8) {
    case 0:  /* 14364 + 8N */
      return ula_mode_x_display_byte_get(row, col);
    
    case 1:  /* 14365 + 8N */
      return ula_mode_x_attribute_byte_get(row, col);

    case 2:  /* 14366 + 8N */
      return ula_mode_x_display_byte_get(row, col + 8);
    
    case 3: /* 14367 + 8N */
      return ula_mode_x_attribute_byte_get(row, col + 8);

    default:
      break;
  }

  return 0xFF;
}


/**
 * Implement floating bus behaviour. This fixes Arkanoid freezing at the
 * start of the first level and prevents flickering and slowdown in
 * Short Circuit.
 */
u8_t ula_floating_bus_read(void) {
  if (ula.tstates_x4 < (ula.display_spec->rows - ula.display_spec->vsync_row) * ula.display_spec->columns) {
    /* Period from IRQ up to and including top border. */
    return 0xFF;
  }

  if (ula.tstates_x4 >= (ula.display_spec->rows - ula.display_spec->vsync_row + 192) * ula.display_spec->columns) {
    /* Period starting with lower border. */
    return 0xFF;
  }

  switch (ula.display_timing) {
    case E_MACHINE_TYPE_ZX_48K:
      return ula_floating_bus_48k_read();

    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      return ula_floating_bus_128k_read();

    default:
      break;
  }

  return 0xFF;
}


u32_t ula_tstates_get(void) {
  return ula.tstates_x4 / 4;
}
