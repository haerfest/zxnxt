#ifndef __CLOCK_H
#define __CLOCK_H


#include "defs.h"


/**
 * https://wiki.specnext.dev/Refresh_Rates
 *
 * "The video mode chosen should always be VGA 0 or RGB at 50Hz. This option of
 * increasing steps VGA 1-6 are for monitors that cannot sync to 50Hz only. All
 * relative timing in the machine is kept the same but the real time speed is
 * higher so programs run faster and sound is higher pitch."
 */
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
  E_CLOCK_CPU_SPEED_3MHZ = 0,  /* 3.5 MHz */
  E_CLOCK_CPU_SPEED_7MHZ,
  E_CLOCK_CPU_SPEED_14MHZ,
  E_CLOCK_CPU_SPEED_28MHZ
} clock_cpu_speed_t;


int                  clock_init(void);
void                 clock_finit(void);
u32_t                clock_28mhz_get(void);
clock_cpu_speed_t    clock_cpu_speed_get(void);
void                 clock_cpu_speed_set(clock_cpu_speed_t speed);
clock_video_timing_t clock_video_timing_get(void);
void                 clock_video_timing_set(clock_video_timing_t timing);
void                 clock_run(u32_t cpu_ticks);
u64_t                clock_ticks(void);


#endif  /* __CLOCK_H */
