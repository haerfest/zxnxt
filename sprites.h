#ifndef __SPRITES_H
#define __SPRITES_H


#include "defs.h"


int  sprites_init(void);
void sprites_finit(void);
void sprites_tick(u32_t row, u32_t column, int* is_enabled, u16_t* rgb);
int  sprites_priority_get(void);
void sprites_priority_set(int is_zero_on_top);
int  sprites_enable_get(void);
void sprites_enable_set(int enable);
int  sprites_enable_over_border_get(void);
void sprites_enable_over_border_set(int enable);
int  sprites_enable_clipping_over_border_get(void);
void sprites_enable_clipping_over_border_set(int enable);
void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void sprites_attribute_set(u8_t slot, u8_t attribute, u8_t value);
void sprites_transparency_index_write(u8_t value);
u8_t sprites_slot_get(void);
void sprites_slot_set(u8_t slot);
void sprites_next_attribute_set(u8_t value);
void sprites_next_pattern_set(u8_t value);
void sprites_palette_set(int use_second);


#endif  /* __SPRITES_H */
