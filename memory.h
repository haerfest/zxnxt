#ifndef __MEMORY_H
#define __MEMORY_H


#include "defs.h"


#define MEMORY_RAM_OFFSET_ZX_SPECTRUM_ROM   0x00000
#define MEMORY_RAM_OFFSET_DIVMMC_ROM        0X10000
#define MEMORY_RAM_OFFSET_ALTROM0_128K      0x18000
#define MEMORY_RAM_OFFSET_ALTROM1_48K       0x1C000
#define MEMORY_RAM_OFFSET_DIVMMC_RAM        0x20000
#define MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM   0x40000


int   memory_init(void);
void  memory_finit(void);
u8_t  memory_read(u16_t address);
void  memory_write(u16_t address, u8_t value);
u8_t* memory_sram(void);
void  memory_refresh_accessors(int page, int n_pages);


#endif  /* __MEMORY_H */
