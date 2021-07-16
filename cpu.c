#include <stdlib.h>
#include <string.h>
#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"  /* For debugging. */
#include "io.h"
#include "log.h"
#include "memory.h"
#include "mf.h"
#include "nextreg.h"

#ifdef TRACE
#include "bootrom.h"
#include "divmmc.h"
#include "mmu.h"
#include "rom.h"
#endif


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


#define CPU_REQUEST_RESET       0x01
#define CPU_REQUEST_IRQ_ULA     0x02
#define CPU_REQUEST_IRQ_LINE    0x04
#define CPU_REQUEST_IRQ         (CPU_REQUEST_IRQ_ULA | CPU_REQUEST_IRQ_LINE)
#define CPU_REQUEST_NMI_MF      0x08
#define CPU_REQUEST_NMI_DIVMMC  0x10
#define CPU_REQUEST_NMI         (CPU_REQUEST_NMI_MF | CPU_REQUEST_NMI_DIVMMC)


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
  reg16_t ir;

  /* Requests from outside. */
  int requests;

  /* IRQ. */
  u8_t im;                      /* Interrupt mode.                                      */
  int  irq_delay;               /* Number of instructions by which IRQ must be delayed. */

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


static void cpu_reset_internal(void) {
  self.requests = 0;

  IFF1 = 0;
  IFF2 = 0;
  PC   = 0;
  I    = 0;
  R    = 0;
  IM   = 0;

  T(3);
}


void cpu_reset(void) {
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
    case E_CPU_IRQ_ULA:
      self.requests = active ? (self.requests | CPU_REQUEST_IRQ_ULA) : (self.requests & ~CPU_REQUEST_IRQ_ULA);
      break;

    case E_CPU_IRQ_LINE:
      self.requests = active ? (self.requests | CPU_REQUEST_IRQ_LINE) : (self.requests & ~CPU_REQUEST_IRQ_LINE);
      break;
  }
}


void cpu_nmi(cpu_nmi_t nmi) {
  switch (nmi) {
    case E_CPU_NMI_MF:
      self.requests |= CPU_REQUEST_NMI_MF;
      break;

    case E_CPU_NMI_DIVMMC:
      self.requests |= CPU_REQUEST_NMI_DIVMMC;
      break;
  }
}


/**
 * http://z80.info/interrup.htm
 *
 * IRQs are level-triggered, so don't clear on handling.
 */
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


static void cpu_nmi_pending(void) {
  /* Save the IFF1 state. */
  IFF2 = IFF1;

  /* Disable further interrupts. */
  IFF1 = 0;

  /* Acknowledge interrupt. */
  T(7);

  /* Save the program counter. */
  memory_write(--SP, PCH); T(3);
  memory_write(--SP, PCL); T(3);

  /* Jump to the NMI routine. */ 
  PC = 0x0066;

  if (self.requests & CPU_REQUEST_NMI_MF) {
    mf_activate();
  }

  /* NMI is edge triggered, so clears on handling. */
  self.requests &= ~CPU_REQUEST_NMI;
}


#ifdef TRACE

static void cpu_sprintf_rom(char* buffer) {
  if (bootrom_is_active()) {
    *buffer = 'B';
  } else if (divmmc_is_active()) {
    *buffer = 'D';
  } else {
    *buffer = '0' + rom_selected();
  }
}


static void cpu_sprintf_flags(char* buffer) {
  buffer[0] = (F & SF_MASK) ? 'S' : '-';
  buffer[1] = (F & ZF_MASK) ? 'Z' : '-';
  buffer[2] = (F & HF_MASK) ? 'H' : '-';
  buffer[3] = (F & VF_MASK) ? 'V' : '-';
  buffer[4] = (F & NF_MASK) ? 'N' : '-';
  buffer[5] = (F & CF_MASK) ? 'C' : '-';
  buffer[6] = 0;
}


static void cpu_trace(void) {
  char rom;
  char flags[6 + 1];
  
  cpu_sprintf_rom(&rom);
  cpu_sprintf_flags(flags);
  log_dbg("%04X R%c %s AF=%04X'%04X BC=%04X'%04X DE=%04X'%04X HL=%04X'%04X IX=%04X IY=%04X SP=%04X DV=%02X MM=%02X %02X %02X %02X %02X %02X %02X %02X\n", PC, rom, flags, AF, AF_, BC, BC_, DE, DE_, HL, HL_, IX, IY, SP, divmmc_control_read(0x00E3), mmu_page_get(0), mmu_page_get(1), mmu_page_get(2), mmu_page_get(3), mmu_page_get(4), mmu_page_get(5), mmu_page_get(6), mmu_page_get(7));
}

#else   /* TRACE */

#define cpu_trace()

#endif  /* TRACE */


void cpu_step(void) {
  cpu_trace();
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
}


u16_t cpu_pc_get(void) {
  return PC;
}
