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
  MAKE_REG(pc, p, c);
  MAKE_REG(sp, s, p);

  /* Interrupt stuff. */
  int iff1;
  int iff2;
} cpu_t;


/* Local-global cpu for fast reference. */
static cpu_t cpu;


/* Convenient flag shortcuts. */
#define SF   cpu.flag.s
#define ZF   cpu.flag.z
#define HF   cpu.flag.h
#define VF   cpu.flag.pv
#define PF   cpu.flag.pv
#define NF   cpu.flag.n
#define CF   cpu.flag.c

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


#define S    cpu.sp.b.s
#define P    cpu.sp.b.p
#define SP   cpu.sp.w

#define PC   cpu.pc.w
#define PCH  cpu.pc.b.p
#define PCL  cpu.pc.b.c

#define IFF1 cpu.iff1
#define IFF2 cpu.iff2


/* Other convenient macros. */
#define SIGN(x)            ((x) & 0x80)
#define HALFCARRY(x,y,z)   (((x) ^ (y) ^ (z)) & 0x10)
#define HALFCARRY16(x,y,z) (((x) ^ (y) ^ (z)) & 0x1000)


int cpu_init(void) {
  cpu_reset();
  return 0;
}


void cpu_finit(void) {
}


void cpu_reset(void) {
  AF   = 0xFFFF;
  SP   = 0xFFFF;
  BC   = 0xFFFF;
  DE   = 0xFFFF;
  HL   = 0xFFFF;
  IX   = 0xFFFF;
  IY   = 0xFFFF;
  IFF1 = 0;
  IFF2 = 0;
  PC   = 0;
  I    = 0;
  R    = 0;
  clock_ticks(3);
}


static void cpu_flags_pack() {
  F = (SF << 7) | (ZF << 6) | (HF << 4) | (VF << 2) | (NF << 1) | CF;
}


static void cpu_flags_unpack() {
  SF = (F & 0x80) >> 7;
  ZF = (F & 0x40) >> 6;
  HF = (F & 0x10) >> 4;
  VF = (F & 0x04) >> 2;
  NF = (F & 0x02) >> 1;
  CF =  F & 0x01;
}


static void cpu_exchange(u16_t* x, u16_t* y) {
  u16_t tmp = *x;
  *x = *y;
  *y = tmp;
}


int cpu_run(u32_t ticks, s32_t* ticks_left) {
  u8_t  opcode;

#include "opcodes.c"

  return 0;
}
