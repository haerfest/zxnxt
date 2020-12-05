#ifndef __ULA_H
#define __ULA_H


#include "clock.h"
#include "defs.h"


typedef enum {
  E_ULA_REFRESH_FREQUENCY_50HZ = 0,
  E_ULA_REFRESH_FREQUENCY_60HZ
} ula_refresh_frequency_t;


int  ula_init(void);
void ula_finit(void);
u8_t ula_read(u16_t address);
void ula_write(u16_t address, u8_t value);
void ula_display_timing_set(clock_display_timing_t timing);
void ula_video_timing_set(clock_video_timing_t timing);
void ula_refresh_frequency_set(ula_refresh_frequency_t frequency);


#endif  /* __ULA_H */
