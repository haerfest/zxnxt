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
  int i;

  const palette_entry_t ula_colours[16] = {
    { 0x00, 0x00, 0x00, 0x00 },  /*  0: Dark black.     */
    { 0x00, 0x00, 0xD7, 0x00 },  /*  1: Dark blue.      */
    { 0xD7, 0x00, 0x00, 0x00 },  /*  2: Dark red.       */
    { 0xD7, 0x00, 0xD7, 0x00 },  /*  3: Dark magenta.   */
    { 0x00, 0xD7, 0x00, 0x00 },  /*  4: Dark green.     */
    { 0x00, 0xD7, 0xD7, 0x00 },  /*  5: Dark cyan.      */
    { 0xD7, 0xD7, 0x00, 0x00 },  /*  6: Dark yellow.    */
    { 0xD7, 0xD7, 0xD7, 0x00 },  /*  7: Dark white.     */
    { 0x00, 0x00, 0x00, 0x00 },  /*  8: Bright black.   */
    { 0x00, 0x00, 0xFF, 0x00 },  /*  9: Bright blue.    */
    { 0xFF, 0x00, 0x00, 0x00 },  /* 10: Bright red.     */
    { 0xFF, 0x00, 0xFF, 0x00 },  /* 11: Bright magenta. */
    { 0x00, 0xFF, 0x00, 0x00 },  /* 12: Bright green.   */
    { 0x00, 0xFF, 0xFF, 0x00 },  /* 13: Bright cyan.    */
    { 0xFF, 0xFF, 0x00, 0x00 },  /* 14: Bright yellow.  */
    { 0xFF, 0xFF, 0xFF, 0x00 }   /* 15: Bright white.   */
  };

  for (i = 0; i < 16; i++) {
    self.palette[E_PALETTE_ULA_FIRST][i] = ula_colours[i];
  }

  return 0;
}


void palette_finit(void) {
}


palette_entry_t palette_read(palette_t palette, u8_t index) {
  return self.palette[palette][index];
}


void palette_write(palette_t palette, u8_t index, palette_entry_t entry) {
  self.palette[palette][index] = entry;
}
