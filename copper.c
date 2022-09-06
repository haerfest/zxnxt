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


static copper_t copper;


int copper_init(void) {
  memset(&copper, 0, sizeof(copper));
  copper_reset(E_RESET_HARD);
  return 0;
}


void copper_finit(void) {
}


void copper_reset(reset_t reset) {
  copper.address    = 0;
  copper.is_running = 0;
}


void copper_data_8bit_write(u8_t value) {
  ((u8_t*) copper.instruction)[copper.address] = value;
  copper.address = (copper.address + 1) & 0x7FF;
}


void copper_data_16bit_write(u8_t value)  {
  if (copper.address & 1) {
    const u16_t even = copper.address - 1;
    copper.instruction[even].msb = copper.cached;
    copper.instruction[even].lsb = value;
  } else {
    copper.cached = value;
  }

  copper.address = (copper.address + 1) & 0x7FF;
}


void copper_address_write(u8_t value)  {
  copper.address = (copper.address & 0x0700) | value;
}


u16_t copper_program_get(u16_t address) {
  return (copper.instruction[address].msb << 8) | copper.instruction[address].lsb;
}


void copper_control_write(u8_t value) {
  copper.address = ((value & 0x07) << 8) | (copper.address & 0x00FF);

  switch (value >> 6) {
    case 0:
      copper.is_running = 0;
      break;

    case 1:
      copper.cpc                    = 0;
      copper.do_move_wait_one_cycle = 0;
      copper.do_reset_pc_on_irq     = 0;
      copper.is_running             = 1;
      break;

    case 2:
      copper.do_reset_pc_on_irq = 0;
      copper.is_running         = 1;
      break;

    case 3:
      copper.cpc                    = 0;
      copper.do_move_wait_one_cycle = 0;
      copper.do_reset_pc_on_irq     = 1;
      copper.is_running             = 1;
      break;
  }
}


void copper_tick(u32_t beam_row, u32_t beam_column, int ticks_28mhz) {
  int    tick;
  u16_t instruction;

  if (!copper.is_running) {
    return;
  }

  for (tick = 0; tick < ticks_28mhz; tick++) {
    instruction = (copper.instruction[copper.cpc].msb << 8) | copper.instruction[copper.cpc].lsb;

    if (instruction == 0x0000) {
      /* NOOP: 1 cycle. */
      copper.cpc = (copper.cpc + 1) & 0x3FF;
    } else if (instruction & 0x8000) {
      /* WAIT: 1 cycle. */
      const u32_t wait_row    = instruction & 0x01FF;
      const u32_t wait_column = (instruction & 0x7E00) >> 5;  /* (>> 6) * 2 b/c sub-pixel beam */
      if (beam_row == wait_row && beam_column >= wait_column) {
        copper.cpc = (copper.cpc + 1) & 0x3FF;
      }
    } else {      
      /* MOVE: 2 cycles. */
      if (copper.do_move_wait_one_cycle) {
        const u8_t reg   = (instruction & 0x7F00) >> 8;
        const u8_t value = instruction & 0x00FF;
        nextreg_write_internal(reg, value);
        copper.cpc = (copper.cpc + 1) & 0x3FF;
      }
      copper.do_move_wait_one_cycle = !copper.do_move_wait_one_cycle;
    }
  }
}


void copper_irq(void) {
  if (copper.do_reset_pc_on_irq) {
    copper.cpc                    = 0;
    copper.do_move_wait_one_cycle = 0;
  }
}
