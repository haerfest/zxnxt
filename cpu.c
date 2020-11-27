#include <stdio.h>
#include "clock.h"
#include "defs.h"
#include "io.h"
#include "memory.h"

#include "cpu.h"


/* Bitmask to (not) set FLAG_P for any byte. */
static const u8_t parity[256] = {
  #include "parity.h"
};


/* TODO: Add a big-endian version. */
#define MAKE_REG(both, high, low) \
  union {                         \
    struct {                      \
      u8_t low;                   \
      u8_t high;                  \
    } b;      /* Byte. */         \
    u16_t w;  /* Word. */         \
  } both


typedef struct {
  /* Combined 8-bit and 16-bit registers. */
  MAKE_REG(af, a, f);
  MAKE_REG(hl, h, l);
  MAKE_REG(bc, b, c);
  MAKE_REG(de, d, e);

  /* Shadow registers. */
  MAKE_REG(af_, a, f);
  MAKE_REG(hl_, h, l);
  MAKE_REG(bc_, b, c);
  MAKE_REG(de_, d, e);

  /* Hidden, internal registers. */
  MAKE_REG(wz, w, z);

  /* Flags. */
  struct {
    int s;
    int z;
    int h;
    int pv;
    int n;
    int c;
  } flag;

  /* Interrupt mode and memory refresh registers. */
  u8_t i;
  u8_t r;

  /* Index registers, split for undocumented opcode support. */
  MAKE_REG(ix, ixh, ixl);
  MAKE_REG(iy, iyh, iyl);

  /* Additional registers. */
  u16_t pc;
  u16_t sp;

  /* Interrupt stuff. */
  int iff1;
  int iff2;
} cpu_t;


/* Local-global cpu for fast reference. */
static cpu_t cpu;


/* Convenient flag shortcuts. */
#define FS   cpu.flag.s
#define FZ   cpu.flag.z
#define FH   cpu.flag.h
#define FV   cpu.flag.pv
#define FP   cpu.flag.pv
#define FN   cpu.flag.n
#define FC   cpu.flag.c

/* Convenient register shortcuts. */
#define A    cpu.af.b.a
#define F    cpu.af.b.f
#define B    cpu.bc.b.b
#define C    cpu.bc.b.c
#define D    cpu.de.b.d
#define E    cpu.de.b.e
#define H    cpu.hl.b.h
#define L    cpu.hl.b.l
#define W    cpu.wz.b.w
#define Z    cpu.wz.b.z

#define AF   cpu.af.w
#define BC   cpu.bc.w
#define DE   cpu.de.w
#define HL   cpu.hl.w
#define WZ   cpu.wz.w

#define AF_  cpu.af_.w
#define BC_  cpu.bc_.w
#define DE_  cpu.de_.w
#define HL_  cpu.hl_.w

#define I    cpu.i
#define R    cpu.r

#define IX   cpu.ix.w
#define IY   cpu.iy.w

#define IXH  cpu.ix.b.ixh
#define IXL  cpu.ix.b.ixl
#define IYH  cpu.iy.b.iyh
#define IYL  cpu.iy.b.iyl


#define PC   cpu.pc
#define SP   cpu.sp

#define IFF1 cpu.iff1
#define IFF2 cpu.iff2


/* Other convenient macros. */
#define SIGN(x)          ((x) & 0x80)
#define HALFCARRY(x,y,z) (((x) ^ (y) ^ (z)) & 0x10)


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
  clock_ticks(3);
}


static void cpu_flags_pack() {
  F = (FS << 7) | (FZ << 6) | (FH << 4) | (FV << 2) | (FN << 1) | FC;
}


static void cpu_flags_unpack() {
  FS = (F & 0x80) >> 7;
  FZ = (F & 0x40) >> 6;
  FH = (F & 0x10) >> 4;
  FV = (F & 0x04) >> 2;
  FN = (F & 0x02) >> 1;
  FC =  F & 0x01;
}


static void cpu_exchange(u16_t* x, u16_t* y) {
  u16_t tmp = *x;
  *x = *y;
  *y = tmp;
}


int cpu_run(u32_t ticks, s32_t* ticks_left) {
  u8_t  tmp8;
  u8_t  opcode;

#include "opcodes.c"

  return 0;
}
