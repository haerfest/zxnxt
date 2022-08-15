#ifndef __MEMORY_H
#define __MEMORY_H


#include "defs.h"


#define MEMORY_SRAM_SIZE                    (2 * 1024 * 1024)

#define MEMORY_RAM_OFFSET_ZX_SPECTRUM_ROM   0x00000
#define MEMORY_RAM_OFFSET_DIVMMC_ROM        0x10000
#define MEMORY_RAM_OFFSET_MF_ROM            0x14000
#define MEMORY_RAM_OFFSET_MF_RAM            0x16000
#define MEMORY_RAM_OFFSET_ALTROM0_128K      0x18000
#define MEMORY_RAM_OFFSET_ALTROM1_48K       0x1C000
#define MEMORY_RAM_OFFSET_DIVMMC_RAM        0x20000
#define MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM   0x40000


int   memory_init(void);
void  memory_finit(void);
u8_t  memory_read(u16_t address);
u8_t  memory_read_opcode(u16_t address);
void  memory_write(u16_t address, u8_t value);
void  memory_contend(u16_t address);
u8_t* memory_sram(void);
void  memory_describe_accessor(int page, const char** reader, const char** writer);
void  memory_refresh_accessors(int page, int n_pages);


#endif  /* __MEMORY_H */
