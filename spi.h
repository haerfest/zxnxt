#ifndef __SPI_H
#define __SPI_H


#include "defs.h"


int  spi_init(void);
void spi_finit(void);
u8_t spi_cs_read(u16_t address);
void spi_cs_write(u16_t address, u8_t value);
u8_t spi_data_read(u16_t address);
void spi_data_write(u16_t address, u8_t value);


#endif  /* __SPI_H */
