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
  u8_t red;
  u8_t green;
  u8_t blue;
  u8_t is_layer2_priority;
} palette_entry_t;


int  palette_init(void);
void palette_finit(void);

palette_entry_t palette_read(palette_t palette, u8_t index);
void            palette_write(palette_t palette, u8_t index, palette_entry_t entry);


#endif  /* __PALETTE_H */
