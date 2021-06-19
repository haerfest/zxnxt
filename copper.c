#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "clock.h"
#include "copper.h"
#include "log.h"
#include "nextreg.h"


/* Instructions are stored in big-endian format. */
typedef struct {
  u8_t msb;
  u8_t lsb;
} instruction_t;


typedef struct {
  u16_t         cpc;      /** 0 - 1023 */
  u16_t         address;  /** 0 - 2047 */
  instruction_t instruction[1024];
  u8_t          lsb;
  int           is_lsb;
  int           is_running;
  int           do_reset_pc_on_irq;
  int           do_move_wait_one_cycle;
} copper_t;


static copper_t self;


int copper_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void copper_finit(void) {
}


void copper_data_8bit_write(u8_t value) {
  ((u8_t*) self.instruction)[self.address] = value;
  self.address = (self.address + 1) & 0x7FF;
}


void copper_data_16bit_write(u8_t value)  {
  if (self.is_lsb) {
    self.lsb = value;
  } else {
    const u16_t even = self.address & 0x7FE;
    self.instruction[even].msb = value;
    self.instruction[even].lsb = self.lsb;
    self.address = (even + 2) & 0x7FF;
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
      self.cpc                = 0;
      self.do_reset_pc_on_irq = 0;
      self.is_running         = 1;
      break;

    case 2:
      self.do_reset_pc_on_irq = 0;
      self.is_running         = 1;
      break;

    case 3:
      self.cpc                = 0;
      self.do_reset_pc_on_irq = 1;
      self.is_running         = 1;
      break;
  }
}


void copper_tick(u32_t beam_row, u32_t beam_column) {
  if (!self.is_running) {
    return;
  }

  const u16_t instruction = (self.instruction[self.cpc].msb << 8) | self.instruction[self.cpc].lsb;

  if (instruction == 0x0000) {
    /* NOOP: 1 cycle. */
    self.cpc = (self.cpc + 1) & 0x3FF;
    return;
  }

  if (instruction & 0x8000) {
    /* WAIT: 1 cycle. */
    const u32_t wait_row    = instruction & 0x01FF;
    const u32_t wait_column = (instruction & 0x7E00) >> 6;
    if (beam_row == wait_row && beam_column >= wait_column) {
      self.cpc = (self.cpc + 1) & 0x3FF;
    }
    return;
  }
  
  /* MOVE: 2 cycles. */
  if (self.do_move_wait_one_cycle) {
    const u8_t reg   = (instruction & 0x7F00) >> 8;
    const u8_t value = instruction & 0x00FF;
    nextreg_write_internal(reg, value);
    self.cpc = (self.cpc + 1) & 0x3FF;
  }
  self.do_move_wait_one_cycle = !self.do_move_wait_one_cycle;
}


void copper_irq(void) {
  if (self.do_reset_pc_on_irq) {
    self.cpc = 0;
  }
}
