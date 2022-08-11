#ifndef __DIVMMC_H
#define __DIVMMC_H


#include "defs.h"


typedef enum {
  E_DIVMMC_ADDR_FIRST = 0,
  E_DIVMMC_ADDR_0000 = E_DIVMMC_ADDR_FIRST,
  E_DIVMMC_ADDR_0008,
  E_DIVMMC_ADDR_0010,
  E_DIVMMC_ADDR_0018,
  E_DIVMMC_ADDR_0020,
  E_DIVMMC_ADDR_0028,
  E_DIVMMC_ADDR_0030,
  E_DIVMMC_ADDR_0038,
  E_DIVMMC_ADDR_0066,
  E_DIVMMC_ADDR_04C6,
  E_DIVMMC_ADDR_04D7,
  E_DIVMMC_ADDR_0562,
  E_DIVMMC_ADDR_056A,
  E_DIVMMC_ADDR_1FF8_1FFF,
  E_DIVMMC_ADDR_3DXX,
  E_DIVMMC_ADDR_LAST = E_DIVMMC_ADDR_3DXX
} divmmc_addr_t;


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
void divmmc_automap_enable(divmmc_addr_t address, int enable);
void divmmc_automap_always(divmmc_addr_t address, int always);
void divmmc_automap_instant(divmmc_addr_t address, int instant);




#endif  /* __DIVMMC_H */
