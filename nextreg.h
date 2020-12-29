#ifndef __NEXTREG_H
#define __NEXTREG_H


#include "defs.h"


typedef enum {
  E_NEXTREG_MACHINE_TYPE_CONFIG_MODE = 0,
  E_NEXTREG_MACHINE_TYPE_ZX_48K,
  E_NEXTREG_MACHINE_TYPE_ZX_128K_PLUS2,
  E_NEXTREG_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3,
  E_NEXTREG_MACHINE_TYPE_PENTAGON
} nextreg_machine_type_t;


int                    nextreg_init(void);
void                   nextreg_finit(void);
void                   nextreg_data_write(u16_t address, u8_t value);
u8_t                   nextreg_data_read(u16_t address);
void                   nextreg_select_write(u16_t address, u8_t value);
u8_t                   nextreg_select_read(u16_t address);
nextreg_machine_type_t nextreg_get_machine_type(void);
u8_t                   nextreg_get_rom_ram_bank(void);
int                    nextreg_is_config_mode_active(void);
int                    nextreg_is_bootrom_active(void);


#endif  /* __NEXTREG_H */
