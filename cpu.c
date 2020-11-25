#include <stdio.h>
#include "clock.h"
#include "defs.h"
#include "memory.h"

#include "cpu.h"


typedef struct {
  /* Normal registers. */
  u8_t a;
  u8_t f;
  u8_t h;
  u8_t l;
  u8_t b;
  u8_t c;
  u8_t d;
  u8_t e;

  /* Shadow registers. */
  u8_t a_;
  u8_t f_;
  u8_t h_;
  u8_t l_;
  u8_t b_;
  u8_t c_;
  u8_t d_;
  u8_t e_;

  /* Index registers. */
  u16_t ix;
  u16_t iy;

  /* Additional registers. */
  u16_t pc;
  u16_t sp;
  u8_t  i;
  u8_t  r;

  /* Interrupt stuff. */
  int iff1;
  int iff2;
} cpu_t;


/* Local-global cpu for fast reference. */
static cpu_t cpu;


int cpu_init(void) {
  cpu_reset();
  return 0;
}


void cpu_finit(void) {
}


void cpu_reset(void) {
  cpu.iff1 = 0;
  cpu.pc   = 0;
  cpu.i    = 0;
  cpu.r    = 0;
  clock_tick(3);
}


int cpu_run(u32_t ticks, s32_t* ticks_left) {
  /* M1 cycle. */
  const u8_t opcode = memory_read(cpu.pc);
  cpu.pc++;
  clock_tick(3);

  switch (opcode) {
    #include "opcodes.c"

    default:
      fprintf(stderr, "Unknown opcode %02Xh at %04Xh\n", opcode, cpu.pc - 1);
      return -1;
  }

  return 0;
}
