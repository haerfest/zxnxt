#include "defs.h"
#include "uart.h"


int uart_init(void) {
  return 0;
}


void uart_finit(void) {
}


void uart_reset(reset_t reset) {
}


u8_t uart_select_read(void) {
  return 0x00;
}


void uart_select_write(u8_t value) {
}


u8_t uart_frame_read(void) {
  return 0x18;
}


void uart_frame_write(u8_t value) {
}


u8_t uart_rx_read(void) {
  return 0x10;  /* tx buffer empty. */
}


void uart_rx_write(u8_t value) {
}


u8_t uart_tx_read(void) {
  return 0x00;
}


void uart_tx_write(u8_t value) {
}
