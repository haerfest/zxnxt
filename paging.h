#ifndef __PAGING_H
#define __PAGING_H


#include "defs.h"


int  paging_init(void);
void paging_finit(void);
void paging_reset(void);
void paging_unlock_spectrum_128k_paging(void);
u8_t paging_spectrum_128k_paging_read(void);
void paging_spectrum_128k_paging_write(u8_t value);
u8_t paging_spectrum_next_bank_extension_read(void);
void paging_spectrum_next_bank_extension_write(u8_t value);
u8_t paging_spectrum_plus_3_paging_read(void);
void paging_spectrum_plus_3_paging_write(u8_t value);


#endif  /* PAGING_H */
