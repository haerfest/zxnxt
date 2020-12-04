#ifndef __DIVMMC_H
#define __DIVMMC_H


#include "defs.h"


int  divmmc_init(void);
void divmmc_finit(void);
void divmmc_control_write(u8_t value);
u8_t divmmc_control_read(void); 


#endif  /* __DIVMMC_H */
