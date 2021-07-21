#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "clock.h"
#include "copper.h"
#include "log.h"
#include "nextreg.h"


/**
 * TODO:
 *
 * https://gitlab.com/thesmog358/tbblue/blob/a0d25589bd28b05c510e528332bcd192bc414ebb/docs/extra-hw/copper/COPPER-v0.1c.TXT
 *
 * > The horizontal dot clock position compare includes an adjustment meaning
 * > that the compare completes three dot clocks early in standard 256x192
 * > resolution and two dot clocks early in Timex HIRES 512x192 resolution. In
 * > practice, a pixel position can be specified with clocks to spare to write a
 * > register value before the pixel is displayed. This saves software having to
 * > auto-adjust positions to arrive early. It also means that a wait for 0,0 can
 * > affect the first pixel of the frame buffer before it is displayed and set
 * > the scroll registers without visual artefacts.
 */


/* #define DUMP_PROGRAM */


/* Instructions are stored in big-endian format. */
typedef struct instruction_t {
  u8_t msb;
  u8_t lsb;
} instruction_t;


typedef struct copper_t {
  u16_t         cpc;      /** 0 - 1023 */
  u16_t         address;  /** 0 - 2047 */
  instruction_t instruction[1024];
  u8_t          cached;
  int           is_running;
  int           do_reset_pc_on_irq;
  int           do_move_wait_one_cycle;
} copper_t;


static copper_t self;


int copper_init(void) {
  memset(&self, 0, sizeof(self));
  copper_reset(E_RESET_HARD);
  return 0;
}


void copper_finit(void) {
}


void copper_reset(reset_t reset) {
  self.address    = 0;
  self.is_running = 0;
}


void copper_data_8bit_write(u8_t value) {
  ((u8_t*) self.instruction)[self.address] = value;
  self.address = (self.address + 1) & 0x7FF;
}


void copper_data_16bit_write(u8_t value)  {
  if (self.address & 1) {
    const u16_t even = self.address - 1;
    self.instruction[even].msb = self.cached;
    self.instruction[even].lsb = value;
  } else {
    self.cached = value;
  }

  self.address = (self.address + 1) & 0x7FF;
}


void copper_address_write(u8_t value)  {
  self.address = (self.address & 0x0700) | value;
}


#ifdef DUMP_PROGRAM

static void copper_dump_program(void) {
  int i;

  for (i = 0; i < 1024; i++) {
    const u16_t instr = (self.instruction[i].msb << 8) | self.instruction[i].lsb;

    log_wrn("copper: %4d: $%04X ", i, instr);

    if (instr == 0x0000) {
      log_wrn("NOOP\n");
    } else if (instr & 0x8000) {
      const u32_t row = instr & 0x01FF;
      const u32_t col = (instr & 0x7E00) >> 6;
      log_wrn("WAIT row=%u col=%u\n", row, col);
      if (instr == 0xFFFF) {
        break;
      }
    } else {
      const u8_t reg   = (instr & 0x7F00) >> 8;
      const u8_t value = instr & 0x00FF;
      log_wrn("MOVE reg=$%02X,value=$%02X\n", reg, value);
    }
  }
}

#endif  /* DUMP_PROGRAM */


void copper_control_write(u8_t value) {
  self.address = ((value & 0x07) << 8) | (self.address & 0x00FF);

  switch (value >> 6) {
    case 0:
      self.is_running = 0;
      break;

    case 1:
      self.cpc                    = 0;
      self.do_move_wait_one_cycle = 0;
      self.do_reset_pc_on_irq     = 0;
      self.is_running             = 1;
      break;

    case 2:
      self.do_reset_pc_on_irq = 0;
      self.is_running         = 1;
      break;

    case 3:
      self.cpc                    = 0;
      self.do_move_wait_one_cycle = 0;
      self.do_reset_pc_on_irq     = 1;
      self.is_running             = 1;
      break;
  }

#ifdef DUMP_PROGRAM
  if (self.is_running) {
    copper_dump_program();
  }
#endif
}


void copper_tick(u32_t beam_row, u32_t beam_column, int ticks_28mhz) {
  int    tick;
  u16_t instruction;

  if (!self.is_running) {
    return;
  }

  for (tick = 0; tick < ticks_28mhz; tick++) {
    instruction = (self.instruction[self.cpc].msb << 8) | self.instruction[self.cpc].lsb;

    if (instruction == 0x0000) {
      /* NOOP: 1 cycle. */
      self.cpc = (self.cpc + 1) & 0x3FF;
    } else if (instruction & 0x8000) {
      /* WAIT: 1 cycle. */
      const u32_t wait_row    = instruction & 0x01FF;
      const u32_t wait_column = (instruction & 0x7E00) >> 6;
      if (beam_row == wait_row && beam_column >= wait_column) {
        self.cpc = (self.cpc + 1) & 0x3FF;
      }
    } else {      
      /* MOVE: 2 cycles. */
      if (self.do_move_wait_one_cycle) {
        const u8_t reg   = (instruction & 0x7F00) >> 8;
        const u8_t value = instruction & 0x00FF;
        nextreg_write_internal(reg, value);
        self.cpc = (self.cpc + 1) & 0x3FF;
      }
      self.do_move_wait_one_cycle = !self.do_move_wait_one_cycle;
    }
  }
}


void copper_irq(void) {
  if (self.do_reset_pc_on_irq) {
    self.cpc                    = 0;
    self.do_move_wait_one_cycle = 0;
  }
}
