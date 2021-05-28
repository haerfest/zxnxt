#ifndef __LAYER2_H
#define __LAYER2_H


#include "defs.h"


int  layer2_init(u8_t* sram);
void layer2_finit(void);
void layer2_reset(void);
void layer2_access_write(u8_t value);
void layer2_control_write(u8_t value);
void layer2_active_bank_write(u8_t bank);
void layer2_shadow_bank_write(u8_t bank);
void layer2_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba);
int  layer2_is_readable(void);
int  layer2_is_writable(void);
u8_t layer2_read(u16_t address);
void layer2_write(u16_t address, u8_t value);
void layer2_palette_set(int use_second);
void layer2_transparency_index_write(u8_t value);
void layer2_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);


#endif  /* __LAYER2_H */
