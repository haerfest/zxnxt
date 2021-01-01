#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"  /* For debugging. */
#include "io.h"
#include "log.h"     /* For debugging. */
#include "memory.h"
#include "mmu.h"     /* For debugging. */
#include "rom.h"     /* For debugging. */


/* Convenient flag shortcuts. */
#define SF_SHIFT 7
#define ZF_SHIFT 6
#define YF_SHIFT 5
#define HF_SHIFT 4
#define XF_SHIFT 3
#define VF_SHIFT 2
#define PF_SHIFT VF_SHIFT
#define NF_SHIFT 1
#define CF_SHIFT 0

#define SF_MASK (1 << SF_SHIFT)
#define ZF_MASK (1 << ZF_SHIFT)
#define YF_MASK (1 << YF_SHIFT)
#define HF_MASK (1 << HF_SHIFT)
#define XF_MASK (1 << XF_SHIFT)
#define VF_MASK (1 << VF_SHIFT)
#define PF_MASK VF_MASK
#define NF_MASK (1 << NF_SHIFT)
#define CF_MASK (1 << CF_SHIFT)

#define SF      (F & SF_MASK)
#define ZF      (F & ZF_MASK)
#define YF      (F & YF_MASK)
#define HF      (F & HF_MASK)
#define XF      (F & XF_MASK)
#define VF      (F & VF_MASK)
#define PF      VF
#define NF      (F & NF_MASK)
#define CF      (F & CF_MASK)

/* Convenient register shortcuts. */
#define A    self.af.b.h
#define F    self.af.b.l
#define B    self.bc.b.h
#define C    self.bc.b.l
#define D    self.de.b.h
#define E    self.de.b.l
#define H    self.hl.b.h
#define L    self.hl.b.l
#define W    self.wz.b.h
#define Z    self.wz.b.l

#define AF   self.af.w
#define BC   self.bc.w
#define DE   self.de.w
#define HL   self.hl.w
#define WZ   self.wz.w

#define AF_  self.af_.w
#define BC_  self.bc_.w
#define DE_  self.de_.w
#define HL_  self.hl_.w

#define IX   self.ix.w
#define IY   self.iy.w

#define IXH  self.ix.b.h
#define IXL  self.ix.b.l
#define IYH  self.iy.b.h
#define IYL  self.iy.b.l

#define S    self.sp.b.h
#define P    self.sp.b.l
#define SP   self.sp.w

#define PC   self.pc.w
#define PCH  self.pc.b.h
#define PCL  self.pc.b.l

#define IFF1 self.iff1
#define IFF2 self.iff2

#define I    self.i
#define R    self.r
#define IM   self.im

#define TMP  self.tmp


/* A shortcut to advance the system clock by a number of CPU ticks. */
#define T    clock_run


#define SZ53(value)   self.sz53[value]
#define SZ53P(value)  self.sz53p[value]


/**
 * All credits to Fuse for this efficent way of looking up the correct HF and
 * VF values. Note that for VF_IDX to work correctly, 'result' should be u16_t,
 * not u8_t.
 */
#define HF_IDX(op1,op2,result) (((op1) & 0x08) >> 3 | ((op2) & 0x08) >> 2 | ((result) & 0x08) >> 1)
#define VF_IDX(op1,op2,result) (((op1) & 0x80) >> 7 | ((op2) & 0x80) >> 6 | ((result) & 0x80) >> 5)

#define HF_ADD(op1,op2,result) hf_add[HF_IDX(op1,op2,result)]
#define HF_SUB(op1,op2,result) hf_sub[HF_IDX(op1,op2,result)]
#define VF_ADD(op1,op2,result) vf_add[VF_IDX(op1,op2,result)]
#define VF_SUB(op1,op2,result) vf_sub[VF_IDX(op1,op2,result)]

static const u8_t hf_add[8] = { 0, HF_MASK, HF_MASK, HF_MASK,       0, 0,       0, HF_MASK };
static const u8_t hf_sub[8] = { 0,       0, HF_MASK,       0, HF_MASK, 0, HF_MASK, HF_MASK };
static const u8_t vf_add[8] = { 0,       0,       0, VF_MASK, VF_MASK, 0,       0,       0 };
static const u8_t vf_sub[8] = { 0, VF_MASK,       0,       0,       0, 0, VF_MASK,       0 };


/* http://graphics.stanford.edu/~seander/bithacks.html#ParityLookupTable */
#define P2(n) n, n^1, n^1, n
#define P4(n) P2(n), P2(n^1), P2(n^1), P2(n)
#define P6(n) P4(n), P4(n^1), P4(n^1), P4(n)

static const u8_t parity[256] = {
    P6(0), P6(1), P6(1), P6(0)
};


/* Simple way to combine two 8-bit registers into a 16-bit one.
 * TODO: Deal with endianness of host CPU. */
typedef union {
  struct {
    u8_t l;  /* Low.  */
    u8_t h;  /* High. */
  } b;       /* Byte. */
  u16_t w;   /* Word. */
} reg16_t;


typedef struct {
  /* Combined 8-bit and 16-bit registers. */
  reg16_t af;
  reg16_t hl;
  reg16_t bc;
  reg16_t de;

  /* Shadow registers. */
  reg16_t af_;
  reg16_t hl_;
  reg16_t bc_;
  reg16_t de_;

  /* Hidden, internal register, aka MEMPTR. */
  reg16_t wz;

  /* Index registers, split for undocumented opcode support. */
  reg16_t ix;
  reg16_t iy;

  /* Additional registers. */
  reg16_t pc;
  reg16_t sp;

  /* Interrupt stuff. */
  int iff1;
  int iff2;

  /* Interrupt table start and memory refresh registers. */
  u8_t i;
  u8_t r;

  /* Interrupt mode. */
  u8_t im;
  int  irq_pending;

  /* Eight-bit register to hold temporary values. */
  u8_t tmp;

  /* Look-up tables to set multiple flags at once and prevent needing to
   * calculate things every time. */
  u8_t sz53[256];
  u8_t sz53p[256];
} cpu_t;


/* Local-global cpu for fast reference. */
static cpu_t self;


#include "opcodes.c"


static void cpu_fill_tables(void) {
  int value;
  
  for (value = 0; value < 256; value++) {
    self.sz53[value]   = (value & 0x80) | (value == 0) << ZF_SHIFT | (value & 0x20) | (value & 0x08);
    self.sz53p[value]  = self.sz53[value] | (1 - parity[value]) << PF_SHIFT;
  }
}


int cpu_init(void) {
  cpu_fill_tables();
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
  IM   = 0;

  self.irq_pending = 0;

  T(3);
}


void cpu_irq(void) {
  if (self.iff1 != 1) {
    return;
  }
  
  if (self.im != 1) {
    log_wrn("cpu: IRQ, but interrupt mode %d not implemented\n", I);
    return;
  }

  self.irq_pending = 1;
}


int cpu_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    cpu_step();

    if (self.irq_pending) {
      self.irq_pending = 0;

      /* The number of cycles required to complete the restart instruction is two
       * more than normal due to the two added wait states. */
      T(2);
      memory_write(--SP, PCH); T(3);
      memory_write(--SP, PCL); T(3);
      PC = 0x0038;
    }
  }

  return 0;
}
