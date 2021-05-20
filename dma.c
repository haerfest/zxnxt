#include <strings.h>
#include "defs.h"
#include "log.h"


typedef struct {
  u8_t* sram;
} dma_t;


static dma_t self;


int dma_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram = sram;

  return 0;
}


void dma_finit(void) {
}


void dma_write(u8_t value) {
  log_dbg("dma: unimplemented DMA write of $%02X\n", value);
}
