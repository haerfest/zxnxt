#ifndef __PALETTE_H
#define __PALETTE_H


#include "defs.h"


/**
 * Packs a 16-bit RRR0GGG0BBB00000 colour
 *  into an 8-bit         RRRGGGBB colour, dropping blue's LSB.
 *
 * Unpack does the opposite, setting blue's LSB to the logical OR of the
 * other blues:
 *
 * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt
 *
 * > The format is RRRGGGBB -  the lower blue bit of the 9-bit colour will be
 * > the logical OR of blue bits 1 and 0 of this 8-bit value.
 */
#define PALETTE_PACK(rgba)   (((rgba) & 0xE000) >> 8 | ((rgba) & 0x0E00) >> 7 | ((rgba) & 0x00C0) >> 6)
#define PALETTE_UNPACK(rgb)  (((rgb)  & 0xE0)   << 8 | ((rgb)  & 0x1C)   << 7 | ((rgb)  & 0x03)   << 6 | (((rgb) & 0x03) != 0) << 5)


typedef enum {
  E_PALETTE_ULA_FIRST = 0,
  E_PALETTE_LAYER2_FIRST,
  E_PALETTE_SPRITES_FIRST,
  E_PALETTE_TILEMAP_FIRST,
  E_PALETTE_ULA_SECOND,
  E_PALETTE_LAYER2_SECOND,
  E_PALETTE_SPRITES_SECOND,
  E_PALETTE_TILEMAP_SECOND
} palette_t;


typedef struct {
  u16_t rgba;
  u8_t  is_layer2_priority;
} palette_entry_t;


int  palette_init(void);
void palette_finit(void);

u16_t           palette_read_rgba(palette_t palette, u8_t index);
palette_entry_t palette_read(palette_t palette, u8_t index);
void            palette_write(palette_t palette, u8_t index, palette_entry_t entry);
int             palette_is_msb_equal(palette_t palette, u8_t index1, u8_t index2);


#endif  /* __PALETTE_H */
