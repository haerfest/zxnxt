#ifndef __CLOCK_H
#define __CLOCK_H


#include "defs.h"


int         clock_init(void);
void        clock_finit(void);
cpu_speed_t clock_cpu_speed_get(void);
void        clock_cpu_speed_set(cpu_speed_t speed);
void        clock_run(u32_t cpu_ticks);
void        clock_run_28mhz_ticks(u64_t ticks);
u64_t       clock_ticks(void);
u32_t       clock_28mhz_get(void);
timing_t    clock_timing_get(void);
u8_t        clock_timing_read(void);
void        clock_timing_write(u8_t value);


#endif  /* __CLOCK_H */
