#ifndef __AY_H
#define __AY_H


#include "defs.h"


int  ay_init(void);
void ay_finit(void);
void ay_reset(reset_t reset);
void ay_register_select(u8_t value);
u8_t ay_register_read(void);
void ay_register_write(u8_t value);
void ay_run(u32_t ticks);
int  ay_turbosound_enable_get(void);
void ay_turbosound_enable_set(int enable);
int  ay_mono_enable_get(int n);
void ay_mono_enable_set(int n, int enable);
int  ay_stereo_acb_get(void);
void ay_stereo_acb_set(int enable);


#endif  /* __AY_H */
