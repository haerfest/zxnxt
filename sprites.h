#ifndef __SPRITES_H
#define __SPRITES_H


#include "defs.h"


int  sprites_init(void);
void sprites_finit(void);
void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba);
void sprites_lockstep_set(int enable);
void sprites_priority_set(int is_zero_on_top);
void sprites_enable_set(int enable);
void sprites_enable_over_border_set(int enable);
void sprites_enable_clipping_over_border_set(int enable);
void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void sprites_attributes_set(u8_t number, u8_t attribute_1, u8_t attribute_2, u8_t attribute_3, u8_t attribute_4, u8_t attribute_5);
void sprites_transparency_index_write(u8_t value);


#endif  /* __SPRITES_H */
