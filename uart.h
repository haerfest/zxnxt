#ifndef __UART_H
#define __UART_H


#include "defs.h"


int  uart_init(void);
void uart_finit(void);
void uart_reset(reset_t reset);
u8_t uart_select_read(void);
void uart_select_write(u8_t value);
u8_t uart_frame_read(void);
void uart_frame_write(u8_t value);
u8_t uart_rx_read(void);
void uart_rx_write(u8_t value);
u8_t uart_tx_read(void);
void uart_tx_write(u8_t value);


#endif  /* __UART_H */
