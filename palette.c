#include "defs.h"
#include "log.h"
#include "palette.h"


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


/**
 * It is likely that the palette is read/written in Next format
 * (each colour component with three bits: RRR, GGG, BBB) far
 * fewer than it is written by the emulator's graphics routines,
 * which expect each component to have eight bits. Therefore
 * the palette entries are stored 'scaled up' to eight bits.
 */


u16_t palette_read_rgba(palette_t palette, u8_t index) {
  return self.palette[palette][index].rgba;
}


palette_entry_t palette_read(palette_t palette, u8_t index) {
  return self.palette[palette][index];
}


void palette_write(palette_t palette, u8_t index, palette_entry_t entry) {
  self.palette[palette][index] = entry;
}


/**
 * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt
 *
 * > Note: This value is 8-bit, so the transparency colour is compared against
 * >       the MSB bits of the final 9-bit colour only.
 */
int palette_is_msb_equal(palette_t palette, u8_t index1, u8_t index2) {
  const u16_t a = self.palette[palette][index1].rgba;
  const u16_t b = self.palette[palette][index2].rgba;

  /* Compare red, green, and blue, ignoring LSB of blue. */
  return (a & 0xFFE0) == (b & 0xFFE0);
}


