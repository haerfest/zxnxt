#ifndef __ULA_H
#define __ULA_H


#include "defs.h"


/**
 * https://wiki.specnext.dev/Refresh_Rates:
 *
 * "With VGA or RGB 50 Hz, 1T = 1/3.5MHz."
 */
typedef enum {
  E_ULA_DISPLAY_TIMING_INTERNAL_USE = 0,
  E_ULA_DISPLAY_TIMING_ZX_48K,     /* 224T * 312 = 69888T per frame = 50.08Hz */
  E_ULA_DISPLAY_TIMING_ZX_128K,    /* 228T * 311 = 70908T per frame = 49.36Hz */
  E_ULA_DISPLAY_TIMING_ZX_PLUS_2A,
  E_ULA_DISPLAY_TIMING_PENTAGON    /* 224T * 320 = 71680T per frame = 48.83Hz */
} ula_display_timing_t;


typedef enum {
  E_ULA_SCREEN_BANK_5 = 5,
  E_ULA_SCREEN_BANK_7 = 7
} ula_screen_bank_t;


int                  ula_init(u8_t* sram);
void                 ula_finit(void);
u8_t                 ula_read(u16_t address);
void                 ula_write(u16_t address, u8_t value);
void                 ula_timex_video_mode_read_enable(int do_enable);
u8_t                 ula_timex_read(u16_t address);
void                 ula_timex_write(u16_t address, u8_t value);
ula_display_timing_t ula_display_timing_get(void);
void                 ula_display_timing_set(ula_display_timing_t timing);
void                 ula_display_frequency_set(int is_60hz);
void                 ula_palette_set(int use_second);
void                 ula_tick(u32_t beam_row, u32_t beam_column);
void                 ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void                 ula_screen_bank_set(ula_screen_bank_t bank);
ula_screen_bank_t    ula_screen_bank_get(void);
int                  ula_contention_get(void);
void                 ula_contention_set(int do_contend);
void                 ula_contend(u8_t bank);
void                 ula_did_complete_frame(void);
u16_t*               ula_frame_buffer_get(void);


#endif  /* __ULA_H */
