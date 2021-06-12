#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "clock.h"
#include "copper.h"
#include "log.h"
#include "nextreg.h"


#define N_INSTRUCTIONS  1024
#define PC_MASK         (2 * N_INSTRUCTIONS - 1)


typedef struct {
  u16_t pc;
  u16_t address;
  u8_t  instruction[N_INSTRUCTIONS * 2];
  u8_t  lsb;
  int   is_lsb;
  int   is_running;
  int   do_reset_pc_on_irq;
  int   do_move_wait_one_cycle;
} copper_t;


static copper_t self;


int copper_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void copper_finit(void) {
}


void copper_data_8bit_write(u8_t value) {
  self.instruction[self.address] = value;
  self.address = (self.address + 1) & PC_MASK;
}


void copper_data_16bit_write(u8_t value)  {
  if (self.is_lsb) {
    self.lsb = value;
  } else {
    const u16_t odd = self.address & (PC_MASK - 1);
    self.instruction[odd + 0] = value;
    self.instruction[odd + 1] = self.lsb;
    self.address = (self.address + 1) & PC_MASK;
  }

  self.is_lsb = !self.is_lsb;
}


void copper_address_write(u8_t value)  {
  self.address = (self.address & 0x0700) | value;
}


void copper_control_write(u8_t value) {
  self.address = ((value & 0x07) << 8) | (self.address & 0x00FF);

  switch (value >> 6) {
    case 0:
      self.is_running = 0;
      break;

    case 1:
      self.pc                 = 0;
      self.do_reset_pc_on_irq = 0;
      self.is_running         = 1;
      break;

    case 2:
      self.do_reset_pc_on_irq = 0;
      self.is_running         = 1;
      break;

    case 3:
      self.pc                 = 0;
      self.do_reset_pc_on_irq = 1;
      self.is_running         = 1;
      break;
  }
}


void copper_tick(u32_t beam_row, u32_t beam_column) {
  u16_t instruction;
  u16_t offset        = self.pc * 2;
  int   do_advance_pc = 1;

  if (!self.is_running) {
    return;
  }

  /* Instructions are stored in big-endian format. */
  instruction = (self.instruction[offset] << 8) | self.instruction[offset + 1];
  
  switch (instruction) {
    case 0x0000:
      /* NOOP: 1 cycle. */
      break;

    case 0xFFFF:
      /* HALT: 1 cycle. */
      self.is_running = 0;
      break;

    default:
      if (instruction & 0x8000) {
        /* WAIT: 1 cycle. */
        const u32_t wait_row    = instruction & 0x01FF;
        const u32_t wait_column = (instruction & 0x7E00) >> 6;
        do_advance_pc           = (beam_row == wait_row && beam_column >= wait_column);
      } else {
        /* MOVE: 2 cycles. */
        self.do_move_wait_one_cycle = !self.do_move_wait_one_cycle;
        do_advance_pc               = !self.do_move_wait_one_cycle;
        if (!self.do_move_wait_one_cycle) {
          const u8_t reg   = (instruction & 0x7F00) >> 8;
          const u8_t value = instruction & 0x00FF;
          nextreg_write_internal(reg, value);
        }
      }
      break;
  }

  if (do_advance_pc) {
    self.pc = (self.pc + 1) & PC_MASK;
  }
}


void copper_irq(void) {
  if (self.do_reset_pc_on_irq) {
    self.pc = 0;
  }
}
