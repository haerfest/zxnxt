#ifndef __MOUSE_H
#define __MOUSE_H


#include "defs.h"


int  mouse_init(void);
void mouse_finit(void);
u8_t mouse_read_x(void);
u8_t mouse_read_y(void);
u8_t mouse_read_buttons(void);
void mouse_refresh(void);


#endif  /* __MOUSE_H */
