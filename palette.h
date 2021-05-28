#ifndef __PALETTE_H
#define __PALETTE_H


#include "defs.h"


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
