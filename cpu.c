#include <stdlib.h>
#include <string.h>
#include "clock.h"
#include "cpu.h"
#include "debug.h"
#include "defs.h"
#include "dma.h"
#include "io.h"
#include "log.h"
#include "memory.h"
#include "mf.h"
#include "nextreg.h"


#include "clock.c"
#include "dma.c"


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

#define I    self.ir.b.h
#define R    self.ir.b.l
#define IR   self.ir.w

#define IM   self.im

#define TMP  self.tmp


/* A shortcut to advance the system clock by a number of CPU ticks. */
#define T    clock_run_inline


#define SZ53(value)   sz53[value]
#define SZ53P(value)  sz53p[value]


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


#define CPU_REQUEST_RESET       0x01
#define CPU_REQUEST_IRQ_ULA     0x02
#define CPU_REQUEST_IRQ_LINE    0x04
#define CPU_REQUEST_IRQ         (CPU_REQUEST_IRQ_ULA | CPU_REQUEST_IRQ_LINE)
#define CPU_REQUEST_NMI_MF      0x08
#define CPU_REQUEST_NMI_DIVMMC  0x10
#define CPU_REQUEST_NMI         (CPU_REQUEST_NMI_MF | CPU_REQUEST_NMI_DIVMMC)


/* Look-up tables to set multiple flags at once and prevent needing to
 * calculate things every time. */
static u8_t sz53[256];
static u8_t sz53p[256];


/* Local-global cpu for fast reference. */
static cpu_t self;


#include "opcodes.c"


static void cpu_fill_tables(void) {
  int value;
  
  for (value = 0; value < 256; value++) {
    sz53[value]  = (value & 0x80) | (value == 0) << ZF_SHIFT | (value & 0x20) | (value & 0x08);
    sz53p[value] = sz53[value] | (1 - parity[value]) << PF_SHIFT;
  }
}


static void cpu_reset_internal(void) {
  self.requests                 = 0;
  self.is_stackless_nmi_enabled = 0;

  IFF1 = 0;
  IFF2 = 0;
  PC   = 0;
  I    = 0;
  R    = 0;
  IM   = 0;

  T(3);
}


void cpu_reset(reset_t reset) {
  self.requests |= CPU_REQUEST_RESET;
}


int cpu_init(void) {
  memset(&self, 0, sizeof(self));

  cpu_fill_tables();

  AF   = 0xFFFF;
  SP   = 0xFFFF;
  BC   = 0xFFFF;
  DE   = 0xFFFF;
  HL   = 0xFFFF;
  IX   = 0xFFFF;
  IY   = 0xFFFF;

  cpu_reset_internal();

  return 0;
}


void cpu_finit(void) {
}


void cpu_irq(cpu_irq_t irq, int active) {
  switch (irq) {
    case E_CPU_IRQ_NONE:
      break;

    case E_CPU_IRQ_ULA:
      self.requests = active ? (self.requests | CPU_REQUEST_IRQ_ULA) : (self.requests & ~CPU_REQUEST_IRQ_ULA);
      break;

    case E_CPU_IRQ_LINE:
      self.requests = active ? (self.requests | CPU_REQUEST_IRQ_LINE) : (self.requests & ~CPU_REQUEST_IRQ_LINE);
      break;
  }
}


void cpu_nmi(cpu_nmi_t nmi, cpu_nmi_source_t source) {
  log_wrn("cpu: NMI (source %d)\n", source);

  switch (nmi) {
    case E_CPU_NMI_NONE:
      break;

    case E_CPU_NMI_MF:
      self.requests |= CPU_REQUEST_NMI_MF;
      self.nmi_source = source;
      break;

    case E_CPU_NMI_DIVMMC:
      self.requests |= CPU_REQUEST_NMI_DIVMMC;
      self.nmi_source = source;
      break;
  }
}


cpu_nmi_t cpu_nmi_get(void) {
  if (self.requests & CPU_REQUEST_NMI_MF) {
    return E_CPU_NMI_MF;
  }
  
  if (self.requests & CPU_REQUEST_NMI_DIVMMC) {
    return E_CPU_NMI_DIVMMC;
  }

  return E_CPU_NMI_NONE;
}


cpu_nmi_source_t cpu_nmi_source(void) {
  return self.nmi_source;
}


/**
 * http://z80.info/interrup.htm
 *
 * IRQs are level-triggered, so don't clear on handling.
 */
inline
static void cpu_irq_pending(void) {
  /* We just executed an EI, need one instruction delay to allow
   * RETN to complete first. */ 
  if (self.irq_delay) {
    return;
  }

  /* Interrupts must be enabled. */
  if (IFF1 == 0) {
    return;
  }

  /* Disable further interrupts. */
  IFF1 = IFF2 = 0;

  /* Save the program counter. */
  --SP;
  T(7);
  memory_write(SP, PCH);
  --SP;
  T(3);
  memory_write(SP, PCL);
  T(3);

  switch (IM) {
    case 0:
      /* Pretend $FF on the bus, which is RST $38, effectively IM 1. */
    case 1:
      PC = 0x0038;
      break;

    default:
    {
      /* Ghouls 'n Ghosts depends on very specific behavior of IM 2. It sets the
       * I register to $81, but places the vector it wants to be jumped to at
       * $81FF and $8200. Lower locations contain either code or all zeros.
       *
       * Hence:
       * - The byte read from the bus must be $FF. If you get this wrong, the
       *   game will freeze at the intro screen, or the Spectrum will reset.
       * - The vector to jump to must be read from $81FF and $8200, not from
       *   $81FE and $81FF as per Zilog's documentation.  If you get this wrong,
       *   music will play but no high scores are shown and you cannot start the
       *   game.
       */
      const u16_t vector = I << 8 | 0xFF;
      PC = memory_read(vector);
      T(3);
      PC |= (memory_read(vector + 1) << 8);
      T(3);
    }
    break;
  }
}


inline
static void cpu_nmi_pending(void) {
  /* Save the IFF1 state. */
  IFF2 = IFF1;

  /* Disable further interrupts. */
  IFF1 = 0;

  /* Acknowledge interrupt. */
  T(7);

  /* Save the program counter. */
  if (self.is_stackless_nmi_enabled) {
    nextreg_write_internal(E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_LSB, PCL);
    nextreg_write_internal(E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_MSB, PCH);
  } else {
    memory_write(--SP, PCH); T(3);
    memory_write(--SP, PCL); T(3);
  }

  /* Jump to the NMI routine. */ 
  PC = 0x0066;

  if (self.requests & CPU_REQUEST_NMI_MF) {
    mf_activate();
  }

  /* NMI is edge triggered, so clears on handling. */
  self.requests &= ~CPU_REQUEST_NMI;
}


int cpu_step(void) {
  dma_run();
  cpu_execute_next_opcode();

  if (self.requests) {
    if (self.requests & CPU_REQUEST_RESET) {
      cpu_reset_internal();
    } else if (self.requests & CPU_REQUEST_NMI) {
      cpu_nmi_pending();
    } else if (self.requests & CPU_REQUEST_IRQ) {
      cpu_irq_pending();
    }
  }

  /* After EI the next RETN must complete before servicing IRQ. */
  if (self.irq_delay) {
    self.irq_delay = 0;
  }

  return debug_is_breakpoint(self.pc.w);
}


int cpu_run(int* do_stop) {
  while (*do_stop == 0) {
    if (cpu_step()) {
      return 1;
    }
  }

  return 0;
}


u16_t cpu_pc_get(void) {
  return PC;
}


cpu_t* cpu_get(void) {
  return &self;
}


int cpu_stackless_nmi_enabled(void) {
  return self.is_stackless_nmi_enabled;
}


void cpu_stackless_nmi_enable(int enable) {
  self.is_stackless_nmi_enabled = enable;
}
