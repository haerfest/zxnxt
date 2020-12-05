#ifndef __ULA_H
#define __ULA_H


#include "defs.h"


typedef enum {
  E_ULA_DISPLAY_TIMING_INTERNAL_USE = 0,
  E_ULA_DISPLAY_TIMING_ZX_48K,
  E_ULA_DISPLAY_TIMING_ZX_128K,
  E_ULA_DISPLAY_TIMING_ZX_PLUS_2A,
  E_ULA_DISPLAY_TIMING_PENTAGON,
  E_ULA_DISPLAY_TIMING_INVALID_5,
  E_ULA_DISPLAY_TIMING_INVALID_6,
  E_ULA_DISPLAY_TIMING_INVALID_7
} ula_display_timing_t;


typedef enum {
  E_ULA_VIDEO_TIMING_VGA_BASE = 0,
  E_ULA_VIDEO_TIMING_VGA_SETTING_1,
  E_ULA_VIDEO_TIMING_VGA_SETTING_2,
  E_ULA_VIDEO_TIMING_VGA_SETTING_3,
  E_ULA_VIDEO_TIMING_VGA_SETTING_4,
  E_ULA_VIDEO_TIMING_VGA_SETTING_5,
  E_ULA_VIDEO_TIMING_VGA_SETTING_6,
  E_ULA_VIDEO_TIMING_HDMI
} ula_video_timing_t;


typedef enum {
  E_ULA_REFRESH_FREQUENCY_50HZ = 0,
  E_ULA_REFRESH_FREQUENCY_60HZ
} ula_refresh_frequency_t;


int  ula_init(void);
void ula_finit(void);
u8_t ula_read(u16_t address);
void ula_write(u16_t address, u8_t value);
void ula_display_timing_set(ula_display_timing_t timing);
void ula_video_timing_set(ula_video_timing_t timing);
void ula_refresh_frequency_set(ula_refresh_frequency_t frequency);


#endif  /* __ULA_H */
