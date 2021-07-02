#ifndef __MAIN_H


#include "defs.h"


u32_t main_next_host_sync_get(u32_t freq_28mhz);
void  main_sync(void);
void  main_show_refresh(int is_60hz);
void  main_show_machine_type(machine_type_t machine);
void  main_show_timing(timing_t timing);
void  main_show_cpu_speed(cpu_speed_t speed);


#endif  /* __MAIN_H */
