#include "defs.h"
#include "log.h"


int i2c_init(void) {
  return 0;
}


void i2c_finit(void) {
}


void i2c_scl_write(u16_t address, u8_t value) {
  log_wrn("i2c: unimplemented write of $%02X to SCL ($%04X)\n", value, address);
}


void i2c_sda_write(u16_t address, u8_t value) {
  log_wrn("i2c: unimplemented write of $%02X to SDA ($%04X)\n", value, address);
}


u8_t i2c_sda_read(u16_t address) {
  log_wrn("i2c: unimplemented read from SDA ($%04X)\n", address);
  return 0xFF;
}
