#ifndef __IO_H
#define __IO_H


#include "defs.h"
#include "mf.h"


typedef enum io_trap_cause_t {
  E_IO_TRAP_CAUSE_NONE = 0,
  E_IO_TRAP_CAUSE_PORT_2FFD_READ,
  E_IO_TRAP_CAUSE_PORT_3FFD_READ,
  E_IO_TRAP_CAUSE_PORT_3FFD_WRITE
} io_trap_cause_t;


int             io_init(void);
void            io_finit(void);
void            io_reset(reset_t reset);
u8_t            io_read(u16_t address);
void            io_write(u16_t address, u8_t value);
void            io_decoding_write(u8_t index, u8_t value);
void            io_traps_enable(int enable);
int             io_are_traps_enabled(void);
void            io_trap_clear();
io_trap_cause_t io_trap_cause(void);
u8_t            io_trap_byte_written(void);
void            io_mf_ports_set(u8_t enable, u8_t disable);


#endif  /* __IO_H */
