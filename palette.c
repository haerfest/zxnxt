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


palette_entry_t palette_read_rgb(palette_t palette, u8_t index) {
  return self.palette[palette][index];
}


palette_entry_t palette_read(palette_t palette, u8_t index) {
  palette_entry_t entry = self.palette[palette][index];
  entry.red   >>= 5;
  entry.green >>= 5;
  entry.blue  >>= 5;
  return entry;
}


void palette_write(palette_t palette, u8_t index, palette_entry_t entry) {
  entry.red   <<= 5;
  entry.green <<= 5;
  entry.blue  <<= 5;
  self.palette[palette][index] = entry;
}
