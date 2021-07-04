#ifndef __ESP_H
#define __ESP_H


#include "defs.h"


int  esp_init(void);
void esp_finit(void);
void esp_reset(reset_t reset);
u8_t esp_tx_read(void);
void esp_tx_write(u8_t value);
u8_t esp_rx_read(void);


#endif  /* __ESP_H */
