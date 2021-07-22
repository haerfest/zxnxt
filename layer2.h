#ifndef __LAYER2_H
#define __LAYER2_H


#include "defs.h"
#include "palette.h"


int  layer2_init(u8_t* sram);
void layer2_finit(void);
void layer2_reset(reset_t reset);
u8_t layer2_access_read(void);
void layer2_access_write(u8_t value);
void layer2_control_write(u8_t value);
void layer2_active_bank_write(u8_t bank);
void layer2_shadow_bank_write(u8_t bank);
int  layer2_is_readable(int page);
int  layer2_is_writable(int page);
u8_t layer2_read(u16_t address);
void layer2_write(u16_t address, u8_t value);
void layer2_palette_set(int use_second);
void layer2_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void layer2_offset_x_msb_write(u8_t value);
void layer2_offset_x_lsb_write(u8_t value);
void layer2_offset_y_write(u8_t value);
void layer2_enable(int enable);


#endif  /* __LAYER2_H */
