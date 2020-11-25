#ifndef __CLOCK_H
#define __CLOCK_H


typedef enum {
  E_CLOCK_DIVIDER_28MHZ   = 1,
  E_CLOCK_DIVIDER_14MHZ   = 2,
  E_CLOCK_DIVIDER_7MHZ    = 4,
  E_CLOCK_DIVIDER_3500KHZ = 8
} clock_divider_t;


typedef enum {
  E_CLOCK_TIMING_VGA_50HZ_48K,
  E_CLOCK_TIMING_VGA_50HZ_128,
  E_CLOCK_TIMING_VGA_50HZ_PENTAGON,
  E_CLOCK_TIMING_VGA_60HZ_48K,
  E_CLOCK_TIMING_VGA_60HZ_128,
  E_CLOCK_TIMING_HDMI_50HZ,
  E_CLOCK_TIMING_HDMI_60HZ
} clock_timing_t;


void clock_init(void);
void clock_finit(void);
void clock_tick(unsigned cpu_ticks);


#endif  /* __CLOCK_H */
