#ifndef __DMA_H
#define __DMA_H


#include "defs.h"


int  dma_init(u8_t* sram);
void dma_finit(void);
void dma_write(u8_t value);


#endif  /* __DMA_H */
