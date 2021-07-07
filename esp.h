#ifndef __ESP_H
#define __ESP_H


#include "defs.h"


int  esp_init(void);
void esp_finit(void);
void esp_reset(reset_t reset);
u8_t esp_tx_read(void);
void esp_tx_write(u8_t value);
u8_t esp_rx_read(void);
void esp_baudrate_set(u32_t baud);
void esp_dataformat_set(int bits_per_frame, int use_parity_check, int use_odd_parity, int use_two_stop_bits);


#endif  /* __ESP_H */
