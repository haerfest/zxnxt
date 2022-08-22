#ifndef __MF_H
#define __MF_H


#include "defs.h"


typedef enum mf_type_t {
  E_MF_TYPE_PLUS_3 = 0,
  E_MF_TYPE_128_V87_2,
  E_MF_TYPE_128_V87_12,
  E_MF_TYPE_1
} mf_type_t;


int       mf_init(u8_t* sram);
void      mf_finit(void);
void      mf_type_set(mf_type_t type);
mf_type_t mf_type_get(void);
void      mf_reset(reset_t reset);
void      mf_activate(void);
void      mf_deactivate(void);
int       mf_is_active(void);
u8_t      mf_enable_read(u16_t address);
void      mf_enable_write(u16_t address, u8_t value);
u8_t      mf_disable_read(u16_t address);
void      mf_disable_write(u16_t address, u8_t value);
u8_t      mf_rom_read(u16_t address);
void      mf_rom_write(u16_t address, u8_t value);
u8_t      mf_ram_read(u16_t address);
void      mf_ram_write(u16_t address, u8_t value);


#endif  /* __MF_H */
