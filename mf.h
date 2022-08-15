#ifndef __MF_H
#define __MF_H


#include "defs.h"


int  mf_init(u8_t* sram);
void mf_finit(void);
void mf_reset(reset_t reset);
void mf_activate(void);
int  mf_is_active(void);
u8_t mf_enable_read(u16_t address);
void mf_enable_write(u16_t address, u8_t value);
u8_t mf_disable_read(u16_t address);
void mf_disable_write(u16_t address, u8_t value);
u8_t mf_rom_read(u16_t address);
void mf_rom_write(u16_t address, u8_t value);
u8_t mf_ram_read(u16_t address);
void mf_ram_write(u16_t address, u8_t value);


#endif  /* __MF_H */
