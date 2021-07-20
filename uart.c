#include <ctype.h>
#include "clock.h"
#include "defs.h"
#include "esp.h"
#include "log.h"
#include "uart.h"


/**
 * TODO Support UARTs redirected to joysticks?
 */
typedef enum {
  E_DEVICE_ESP = 0,
  E_DEVICE_PI
} device_t;


typedef struct {
  u32_t prescalar;
  int   bits_per_frame;
  int   use_parity_check;
  int   use_odd_parity;
  int   use_two_stop_bits;
} uart_t;


typedef struct {
  uart_t   uart[2];
  device_t selected;
} uarts_t;


static uarts_t self;


int uart_init(void) {
  return 0;
}


void uart_finit(void) {
}


static u32_t baudrate(u32_t prescalar) {
  return prescalar ? (clock_28mhz_get() / prescalar) : clock_28mhz_get();
}


void uart_reset(reset_t reset) {
  self.selected = E_DEVICE_ESP;

  if (reset == E_RESET_HARD) {
    int i;

    for (i = 0; i < 2; i++) {
      self.uart[i].prescalar         = 0;
      self.uart[i].bits_per_frame    = 8;
      self.uart[i].use_parity_check  = 0;
      self.uart[i].use_odd_parity    = 0;
      self.uart[i].use_two_stop_bits = 0;
    }
  }

  esp_baudrate_set(baudrate(self.uart[self.selected].prescalar));
  esp_dataformat_set(self.uart[E_DEVICE_ESP].bits_per_frame, self.uart[E_DEVICE_ESP].use_parity_check, self.uart[E_DEVICE_ESP].use_odd_parity, self.uart[E_DEVICE_ESP].use_two_stop_bits);
}


u8_t uart_select_read(void) {
  return (self.selected << 6) | (self.uart[self.selected].prescalar >> 14);
}


void uart_select_write(u8_t value) {
  self.selected = (value & 0x40) >> 6;

  if (value & 0x10) {
    self.uart[self.selected].prescalar = (self.uart[self.selected].prescalar & 0x3FFF) | ((value & 0x07) << 14);
    if (self.selected == E_DEVICE_ESP) {
      esp_baudrate_set(self.uart[E_DEVICE_ESP].prescalar);
    }
  }

  log_wrn("uart%d: selected, prescalar=%u => %u baud \n",
          self.selected,
          self.uart[self.selected].prescalar,
          self.uart[self.selected].prescalar == 0 ? 0 : clock_28mhz_get() / self.uart[self.selected].prescalar);
}


u8_t uart_frame_read(void) {
  const uart_t* uart = &self.uart[self.selected];

  return ((uart->bits_per_frame - 5) << 3)
       |  (uart->use_parity_check    << 2)
       |  (uart->use_odd_parity      << 1)
       |   uart->use_two_stop_bits;
}


void uart_frame_write(u8_t value) {
  uart_t* uart = &self.uart[self.selected];

  uart->bits_per_frame    = ((value & 0x18) >> 3) + 5;
  uart->use_parity_check  =  (value & 0x04) >> 2;
  uart->use_odd_parity    =  (value & 0x02) >> 1;
  uart->use_two_stop_bits =   value & 0x01;

  if (self.selected == E_DEVICE_ESP) {
    esp_dataformat_set(uart->bits_per_frame, uart->use_parity_check, uart->use_odd_parity, uart->use_two_stop_bits);
  }

  log_wrn("uart%d: bits/frame=%d parity=%c type=%c stop=%d\n",
          self.selected,
          self.uart[self.selected].bits_per_frame,
          self.uart[self.selected].use_parity_check  ? 'Y' : 'N',
          self.uart[self.selected].use_odd_parity    ? 'O' : 'E',
          self.uart[self.selected].use_two_stop_bits ? 2 : 1);
}


u8_t uart_rx_read(void) {
  switch (self.selected) {
    case E_DEVICE_ESP:
      return esp_rx_read();

    default:
      break;
  }
  
  return 0x10;  /* tx buffer empty. */
}


void uart_rx_write(u8_t value) {
  self.uart[self.selected].prescalar = (value & 0x80)
    ? (self.uart[self.selected].prescalar & 0x1C07F) | ((value & 0x7F) << 7)
    : (self.uart[self.selected].prescalar & 0x1FF80) |  (value & 0x7F);

  if (self.selected == E_DEVICE_ESP) {
    esp_baudrate_set(baudrate(self.uart[self.selected].prescalar));
  }

  log_wrn("uart%d: prescalar=%u => %u baud\n",
          self.selected,
          self.uart[self.selected].prescalar,
          self.uart[self.selected].prescalar == 0 ? 0 : clock_28mhz_get() / self.uart[self.selected].prescalar);
}


u8_t uart_tx_read(void) {
  switch (self.selected) {
    case E_DEVICE_ESP:
      return esp_tx_read();

    default:
      break;
  }

  return 0x00;
}


void uart_tx_write(u8_t value) {
  switch (self.selected) {
    case E_DEVICE_ESP:
      /* log_wrn("%c", isprint(value) ? value : '.'); */
      esp_tx_write(value);
      break;

    default:
      break;
  }
}
