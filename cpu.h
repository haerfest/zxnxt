#ifndef __CPU_H
#define __CPU_H


#include "defs.h"


typedef enum {
  E_CPU_IRQ_NONE,
  E_CPU_IRQ_ULA,
  E_CPU_IRQ_LINE
} cpu_irq_t;


typedef enum {
  E_CPU_NMI_NONE,
  E_CPU_NMI_MF,
  E_CPU_NMI_DIVMMC
} cpu_nmi_t;


typedef enum {
  E_CPU_NMI_SOURCE_OTHER,
  E_CPU_NMI_SOURCE_TRAP,
  E_CPU_NMI_SOURCE_NEXTREG
} cpu_nmi_source_t;


/* Simple way to combine two 8-bit registers into a 16-bit one.
 * TODO: Deal with endianness of host CPU. */
typedef union reg16_t {
  struct {
    u8_t l;  /* Low.  */
    u8_t h;  /* High. */
  } b;       /* Byte. */
  u16_t w;   /* Word. */
} reg16_t;


typedef struct cpu_t {
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
  int              requests;
  cpu_nmi_source_t nmi_source;

  /* IRQ. */
  u8_t im;                      /* Interrupt mode.                                      */
  int  irq_delay;               /* Number of instructions by which IRQ must be delayed. */

  /* Eight-bit register to hold temporary values. */
  u8_t tmp;

} cpu_t;


int              cpu_init(void);
void             cpu_finit(void);
void             cpu_run(int* do_stop);
void             cpu_reset(void);
void             cpu_irq(cpu_irq_t irq, int active);
void             cpu_nmi(cpu_nmi_t nmi, cpu_nmi_source_t source);
cpu_nmi_t        cpu_nmi_get(void);
cpu_nmi_source_t cpu_nmi_source(void);
u16_t            cpu_pc_get(void);
cpu_t*           cpu_get(void);


#endif  /* __CPU_H */
