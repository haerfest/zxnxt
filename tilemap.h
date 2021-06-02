#ifndef __TILEMAP_H
#define __TILEMAP_H


#include "defs.h"


int    tilemap_init(u8_t* sram);
void   tilemap_finit(void);
void   tilemap_tilemap_control_write(u8_t value);
void   tilemap_default_tilemap_attribute_write(u8_t value);
void   tilemap_tilemap_base_address_write(u8_t value);
void   tilemap_tilemap_tile_definitions_address_write(u8_t value);
void   tilemap_transparency_index_write(u8_t value);
void   tilemap_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba);
void   tilemap_offset_x_msb_write(u8_t value);
void   tilemap_offset_x_lsb_write(u8_t value);
void   tilemap_offset_y_write(u8_t value);
int    tilemap_priority_over_ula_get(u32_t row, u32_t column);
void   tilemap_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void   tilemap_transparency_colour_write(u8_t rgb);


#endif  /* __TILEMAP_H */
