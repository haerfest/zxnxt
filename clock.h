#ifndef __CLOCK_H
#define __CLOCK_H


#include "defs.h"


typedef enum {
  E_CLOCK_TIMING_FIRST     = 0,
  E_CLOCK_TIMING_VGA_BASE = E_CLOCK_TIMING_FIRST,
  E_CLOCK_TIMING_VGA_1,
  E_CLOCK_TIMING_VGA_2,
  E_CLOCK_TIMING_VGA_3,
  E_CLOCK_TIMING_VGA_4,
  E_CLOCK_TIMING_VGA_5,
  E_CLOCK_TIMING_VGA_6,
  E_CLOCK_TIMING_HDMI,
  E_CLOCK_TIMING_LAST = E_CLOCK_TIMING_HDMI
} clock_timing_t;


typedef enum {
  E_CLOCK_CPU_SPEED_3MHZ = 0,  /* 3.5 MHz */
  E_CLOCK_CPU_SPEED_7MHZ,
  E_CLOCK_CPU_SPEED_14MHZ,
  E_CLOCK_CPU_SPEED_28MHZ
} clock_cpu_speed_t;


int               clock_init(void);
void              clock_finit(void);
clock_cpu_speed_t clock_cpu_speed_get(void);
void              clock_cpu_speed_set(clock_cpu_speed_t speed);
void              clock_run(u32_t cpu_ticks);
u64_t             clock_ticks(void);
clock_timing_t    clock_timing_get(void);
u8_t              clock_timing_read(void);
void              clock_timing_write(u8_t value);
u32_t             clock_28mhz_get(void);


#endif  /* __CLOCK_H */
