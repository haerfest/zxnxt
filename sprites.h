#ifndef __SPRITES_H
#define __SPRITES_H


#include "defs.h"


int  sprites_init(void);
void sprites_finit(void);
void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba);
void sprites_priority_set(int is_zero_on_top);
void sprites_enable_set(int enable);
void sprites_enable_over_border_set(int enable);
void sprites_enable_clipping_over_border_set(int enable);
void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void sprites_attribute_set(u8_t number, int attribute, u8_t value);
void sprites_transparency_index_write(u8_t value);
u8_t sprites_number_get(void);
void sprites_number_set(u8_t number);
void sprites_next_attribute_set(u8_t value);
void sprites_next_pattern_set(u8_t value);


#endif  /* __SPRITES_H */
