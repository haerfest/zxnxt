#ifndef __AY_H
#define __AY_H


#include "defs.h"


int  ay_init(void);
void ay_finit(void);
void ay_reset(void);
void ay_register_select(u8_t value);
u8_t ay_register_read(void);
void ay_register_write(u8_t value);
void ay_run(u32_t ticks);


#endif  /* __AY_H */
