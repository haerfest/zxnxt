#ifndef __DIVMMC_H
#define __DIVMMC_H


#include "defs.h"


int  divmmc_init(u8_t* sram);
void divmmc_finit(void);
void divmmc_reset(reset_t reset);
int  divmmc_is_active(void);
u8_t divmmc_ram_read(u16_t address);
void divmmc_ram_write(u16_t address, u8_t value);
u8_t divmmc_rom_read(u16_t address);
void divmmc_rom_write(u16_t address, u8_t value);
u8_t divmmc_control_read(u16_t address); 
void divmmc_control_write(u16_t address, u8_t value);
int  divmmc_is_automap_enabled(void);
void divmmc_automap_enable(int enable);
void divmmc_automap_on_fetch_enable(u16_t address, int enable);
void divmmc_automap_on_fetch_always(u16_t address, int always);
void divmmc_automap_on_fetch_instant(u16_t address, int instant);
void divmmc_automap(u16_t address, int instant);
void divmmc_mapram_reset(void);


#endif  /* __DIVMMC_H */
