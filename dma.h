#ifndef __DMA_H
#define __DMA_H


#include "defs.h"


int  dma_init(u8_t* sram);
void dma_finit(void);
void dma_reset(reset_t reset);
void dma_write(u16_t address, u8_t value);
u8_t dma_read(u16_t address);
void dma_run(void);


#endif  /* __DMA_H */
