#include "defs.h"
#include "mmu.h"
#include "memory.h"


int memory_init(void) {
  return 0;
}


void memory_finit(void) {
}


u8_t memory_read(u16_t address) {
  return mmu_read(address);
}


void memory_write(u16_t address, u8_t value) {
  mmu_write(address, value);
}
