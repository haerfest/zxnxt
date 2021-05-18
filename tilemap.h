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
void   tilemap_tick(u32_t beam_row, u32_t beam_column);
u16_t* tilemap_frame_buffer_get(void);


#endif  /* __TILEMAP_H */
