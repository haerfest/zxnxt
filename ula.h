#ifndef __ULA_H
#define __ULA_H


#include "clock.h"
#include "defs.h"
#include "palette.h"


typedef enum {
  E_ULA_SCREEN_BANK_5 = 5,
  E_ULA_SCREEN_BANK_7 = 7
} ula_screen_bank_t;


int               ula_init(u8_t* sram);
void              ula_finit(void);
void              ula_reset(reset_t reset);
u8_t              ula_read(u16_t address);
void              ula_write(u16_t address, u8_t value);
void              ula_timex_video_mode_read_enable(int do_enable);
u8_t              ula_timex_read(u16_t address);
void              ula_timex_write(u16_t address, u8_t value);
machine_type_t    ula_timing_get(void);
void              ula_timing_set(machine_type_t machine);
void              ula_palette_set(int use_second);
void              ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void              ula_screen_bank_set(ula_screen_bank_t bank);
ula_screen_bank_t ula_screen_bank_get(void);
int               ula_contention_get(void);
void              ula_contention_set(int do_contend);
void              ula_contend(u8_t bank);
void              ula_enable_set(int enable);
void              ula_did_complete_frame(void);
void              ula_tick(u32_t row, u32_t column, int* is_enabled, int* is_border, int* is_clipped, const palette_entry_t** rgb);
void              ula_transparency_colour_write(u8_t rgb);
void              ula_attribute_byte_format_write(u8_t value);
u8_t              ula_attribute_byte_format_read(void);
void              ula_next_mode_enable(int do_enable);
void              ula_60hz_set(int enable);
int               ula_60hz_get(void);
int               ula_irq_enable_get(void);
void              ula_irq_enable_set(int enable);
void              ula_lo_res_enable_set(int enable);
void              ula_lo_res_offset_x_write(u8_t value);
void              ula_lo_res_offset_y_write(u8_t value);
int               ula_beam_to_frame_buffer(u32_t beam_row, u32_t beam_column, u32_t* frame_buffer_row, u32_t* frame_buffer_column);
void              ula_hdmi_enable(int enable);


#endif  /* __ULA_H */
