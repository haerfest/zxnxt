#ifndef __SDCARD_H
#define __SDCARD_H


#include "defs.h"


typedef enum {
  E_SDCARD_0 = 0,
  E_SDCARD_1
} sdcard_nr_t;


int  sdcard_init(void);
void sdcard_finit(void);
u8_t sdcard_read(sdcard_nr_t card, u16_t address);
void sdcard_write(sdcard_nr_t card, u16_t address, u8_t value);


#endif  /* __SDCARD_H */
