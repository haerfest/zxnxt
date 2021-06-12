#include "clock.h"
#include "copper.h"
#include "log.h"


void copper_init(void) {
}


void copper_finit(void) {
}


void copper_data_8bit_write(u8_t value) {
  log_wrn("copper: unimplemented write of $%02X to data-8bit\n", value);
}


void copper_data_16bit_write(u8_t value)  {
  log_wrn("copper: unimplemented write of $%02X to data-16bit\n", value);
}


void copper_address_write(u8_t value)  {
  log_wrn("copper: unimplemented write of $%02X to address\n", value);
}


void copper_control_write(u8_t value) {
  log_wrn("copper: unimplemented write of $%02X to control\n", value);
}
