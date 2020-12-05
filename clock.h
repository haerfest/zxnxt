#ifndef __CLOCK_H
#define __CLOCK_H


#include "defs.h"


typedef enum {
  E_CLOCK_DISPLAY_TIMING_INTERNAL_USE = 0,
  E_CLOCK_DISPLAY_TIMING_ZX_48K,
  E_CLOCK_DISPLAY_TIMING_ZX_128K,
  E_CLOCK_DISPLAY_TIMING_ZX_PLUS_2A,
  E_CLOCK_DISPLAY_TIMING_PENTAGON,
  E_CLOCK_DISPLAY_TIMING_INVALID_5,
  E_CLOCK_DISPLAY_TIMING_INVALID_6,
  E_CLOCK_DISPLAY_TIMING_INVALID_7
} clock_display_timing_t;


typedef enum {
  E_CLOCK_VIDEO_TIMING_VGA_BASE = 0,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_1,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_2,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_3,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_4,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_5,
  E_CLOCK_VIDEO_TIMING_VGA_SETTING_6,
  E_CLOCK_VIDEO_TIMING_HDMI
} clock_video_timing_t;


typedef enum {
  E_CLOCK_CPU_SPEED_3MHZ = 0,  /* 3.5 or 3.5469 MHz */
  E_CLOCK_CPU_SPEED_7MHZ,
  E_CLOCK_CPU_SPEED_14MHZ,
  E_CLOCK_CPU_SPEED_28MHZ
} clock_cpu_speed_t;


/* Called from the clock whenever 'ticks' 28 MHz master clock ticks have elapsed. */
typedef void (*clock_callback_t)(u64_t ticks, unsigned int delta);


int                    clock_init(void);
void                   clock_finit(void);
void                   clock_cpu_speed_set(clock_cpu_speed_t speed);
clock_display_timing_t clock_display_timing_get(void);
void                   clock_display_timing_set(clock_display_timing_t timing);
clock_video_timing_t   clock_video_timing_get(void);
void                   clock_video_timing_set(clock_video_timing_t timing);
int                    clock_register_callback(clock_callback_t callback);
void                   clock_ticks(unsigned int cpu_ticks);

#endif  /* __CLOCK_H */
