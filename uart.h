#ifndef __UART_H
#define __UART_H


#include "defs.h"


int  uart_init(void);
void uart_finit(void);
void uart_reset(reset_t reset);


#endif  /* __UART_H */
