#ifndef __I2C_H
#define __I2C_H


#include "defs.h"


int  i2c_init(void);
void i2c_finit(void);
void i2c_reset(reset_t reset);
void i2c_scl_write(u16_t address, u8_t value);
void i2c_sda_write(u16_t address, u8_t value);
u8_t i2c_sda_read(u16_t address);


#endif  /* __I2C_H */
