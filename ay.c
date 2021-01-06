#include "defs.h"
#include "log.h"


int ay_init(void) {
  return 0;
}


void ay_finit(void) {
}


void ay_register_select(u8_t value) {
  log_dbg("ay: unimplemented write of $%02X to REGISTER port\n", value);
}


u8_t ay_register_read(void) {
  log_dbg("ay: unimplemented read from REGISTER port\n");
  return 0xFF;
}


void ay_register_write(u8_t value) {
  log_dbg("ay: unimplemented write of $%02X to DATA port\n", value);
}
