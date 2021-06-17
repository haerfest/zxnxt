#include "defs.h"
#include "log.h"
#include "palette.h"


#define RGB8_TO_RGB9(rgb8)  (((rgb8) << 1) | (((rgb8) & 2) >> 1) | ((rgb8) & 1))
#define RGB9_TO_RGB16(rgb9) ((((rgb9) & 0x1C0) << 7) | (((rgb9) & 0x38) << 6) | (((rgb9) & 0x07) << 5))


#define N_PALETTES  (E_PALETTE_TILEMAP_SECOND - E_PALETTE_ULA_FIRST + 1)
#define N_ENTRIES   256


typedef struct {
  palette_entry_t palette[N_PALETTES][N_ENTRIES];
} self_t;


static self_t self;


int palette_init(void) {
  return 0;
}


void palette_finit(void) {
}


const palette_entry_t* palette_read(palette_t palette, u8_t index) {
  return &self.palette[palette][index];
}


void palette_write_rgb8(palette_t palette, u8_t index, u8_t value) {
  palette_entry_t* entry = &self.palette[palette][index];

  /**
   * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt
   *
   * > The format is RRRGGGBB -  the lower blue bit of the 9-bit colour will be
   * > the logical OR of blue bits 1 and 0 of this 8-bit value.
   */
  entry->rgb8               = value;
  entry->rgb9               = RGB8_TO_RGB9(value);
  entry->rgb16              = RGB9_TO_RGB16(entry->rgb9);
  entry->is_layer2_priority = 0;
}


void palette_write_rgb9(palette_t palette, u8_t index, u8_t value) {
  palette_entry_t* entry = &self.palette[palette][index];

  entry->rgb9               = (entry->rgb9 & 0x1FE) | (value & 1);
  entry->rgb16              = RGB9_TO_RGB16(entry->rgb9);
  entry->is_layer2_priority = value >> 7;
}


u16_t palette_rgb8_rgb16(u8_t rgb8) {
  const u16_t rgb9 = RGB8_TO_RGB9(rgb8);
  return RGB9_TO_RGB16(rgb9);
}
