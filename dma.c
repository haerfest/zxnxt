#include <strings.h>
#include "clock.h"
#include "defs.h"
#include "dma.h"
#include "io.h"
#include "log.h"
#include "memory.h"


/**
 * Note: implements zxnDMA only, not Z80 DMA.
 */


#define NO_GROUP  0xFF
#define N_GROUPS  7


typedef enum {
  E_DMA_CMD_RESET                        = 0xC3,
  E_DMA_CMD_RESET_PORT_A_TIMING          = 0xC7,
  E_DMA_CMD_RESET_PORT_B_TIMING          = 0xCB,
  E_DMA_CMD_LOAD                         = 0xCF,
  E_DMA_CMD_CONTINUE                     = 0xD3,
  E_DMA_CMD_DISABLE_INTERRUPTS           = 0xAF,
  E_DMA_CMD_ENABLE_INTERRUPTS            = 0xAB,
  E_DMA_CMD_RESET_AND_DISABLE_INTERRUPTS = 0xA3,
  E_DMA_CMD_ENABLE_AFTER_RTI             = 0xB7,
  E_DMA_CMD_READ_STATUS_BYTE             = 0xBF,
  E_DMA_CMD_REINITIALISE_STATUS_BYTE     = 0x8B,
  E_DMA_CMD_INITIATE_READ_SEQUENCE       = 0xA7,
  E_DMA_CMD_FORCE_READY                  = 0xB3,
  E_DMA_CMD_ENABLE_DMA                   = 0x87,
  E_DMA_CMD_DISABLE_DMA                  = 0x83,
  E_DMA_CMD_READ_MASK_FOLLOWS            = 0xBB
} dma_cmd_t;


typedef enum {
  E_MODE_BYTE = 0,
  E_MODE_CONTINUOUS,
  E_MODE_BURST,
  E_MODE_DO_NOT_PROGRAM
} mode_t;


typedef void (*dma_decode_handler_t)(u8_t value, int is_first_write);


typedef struct {
  u8_t*  sram;
  u8_t   group;
  u8_t   group_value[N_GROUPS];
  u16_t  a_starting_address;
  u16_t  b_starting_address;
  u8_t   a_variable_timing;
  u8_t   b_variable_timing;
  u16_t  block_length;
  u8_t   interrupt_control;
  u8_t   pulse_control;
  u8_t   interrupt_vector;
  u8_t   read_mask;
  u8_t   mask_byte;
  u8_t   match_byte;
  u8_t   zxn_prescalar;
  int    is_enabled;
  u16_t  n_transferred;
  mode_t mode;
  int    do_restart;
  u64_t  next_transfer_ticks;
  
  u16_t src_address;
  u16_t dst_address;
  int   is_src_io;
  int   is_dst_io;
  int   src_address_delta;
  int   dst_address_delta;
  u8_t  src_cycle_length;
  u8_t  dst_cycle_length;
} dma_t;


static dma_t self;


int dma_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram = sram;

  dma_reset(E_RESET_HARD);

  return 0;
}


void dma_finit(void) {
}


void dma_reset(reset_t reset) {
  self.group      = NO_GROUP;
  self.read_mask  = 0x7F;
  self.is_enabled = 0;
}


static int dma_get_address_delta(int group) {
  switch ((self.group_value[group] & 0x30) >> 4) {
    case 0:
      return -1;

    case 1:
      return +1;

    default:
      break;
  }

  return 0;
}


static void dma_load(void) {
  const int   is_a_io         = self.group_value[1] & 0x08;
  const int   is_b_io         = self.group_value[2] & 0x08;
  const int   a_address_delta = dma_get_address_delta(1);
  const int   b_address_delta = dma_get_address_delta(2);
  const u8_t  a_cycle_length  = self.a_variable_timing & 0x03;
  const u8_t  b_cycle_length  = self.b_variable_timing & 0x03;
  const int   is_a_to_b       = self.group_value[0] & 0x04;

  if (is_a_to_b) {
    /* A -> B */
    self.src_address       = self.a_starting_address;
    self.dst_address       = self.b_starting_address;
    self.is_src_io         = is_a_io;
    self.is_dst_io         = is_b_io;
    self.src_address_delta = a_address_delta;
    self.dst_address_delta = b_address_delta;
    self.src_cycle_length  = a_cycle_length;
    self.dst_cycle_length  = b_cycle_length;
  } else {
    /* B -> A */
    self.src_address       = self.b_starting_address;
    self.dst_address       = self.a_starting_address;
    self.is_src_io         = is_b_io;
    self.is_dst_io         = is_a_io;
    self.src_address_delta = b_address_delta;
    self.dst_address_delta = a_address_delta;
    self.src_cycle_length  = b_cycle_length;
    self.dst_cycle_length  = a_cycle_length;
  }

  self.n_transferred = 0;

  {
    const char* delta_str[] = {"--", "", "++"};
    const char* mode_str[] = {"BYTE", "CONT", "BRST", "N/A"};

    log_wrn("dma: LOAD %s %s>%s %04X%s>%04X%s %s len=%04X pre=%02X\n",
            is_a_to_b ? "A>B" : "B>A",
            self.is_src_io ? "IO" : "M",
            self.is_dst_io ? "IO" : "M",
            self.src_address,
            delta_str[1 + self.src_address_delta],
            self.dst_address,
            delta_str[1 + self.dst_address_delta],
            mode_str[self.mode],
            self.block_length,
            self.zxn_prescalar);
  }          
}


static u8_t dma_decode_group(u8_t value) {
  switch (value & 0x83) {
    case 0x01:
    case 0x02:
    case 0x03:
      return 0;

    case 0x80:
      return 3;

    case 0x81:
      return 4;

    case 0x83:
      return 6;

    default:
      break;
  }

  switch (value & 0x87) {
    case 0x00:
      return 2;

    case 0x04:
      return 1;

    default:
      break;
  }
  
  if ((value & 0xC7) == 0x82) {
    return 5;
  }

  return NO_GROUP;
}


static void dma_group_0_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[0] & 0x08) {
      self.a_starting_address &= 0xFF00;
      self.a_starting_address |= value;
      self.group_value[0]     &= ~0x08;
    } else if (self.group_value[0] & 0x10) {
      self.a_starting_address &= 0x00FF;
      self.a_starting_address |= value << 8;
      self.group_value[0]     &= ~0x10;
    } else if (self.group_value[0] & 0x20) {
      self.block_length &= 0xFF00;
      self.block_length |= value;
      self.group_value[0] &= ~0x20;
    } else if (self.group_value[0] & 0x40) {
      self.block_length   &= 0x00FF;
      self.block_length   |= value << 8;
      self.group_value[0] &= ~0x40;
    }
  }

  if ((self.group_value[0] & 0x78) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_1_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[1] & 0x40) {
      self.a_variable_timing = value;
      self.group_value[1]   &= ~0x40;
    }
  }

  if ((self.group_value[1] & 0x40) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_2_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[2] & 0x40) {
      self.b_variable_timing = value;
      self.group_value[2]   &= ~0x40;
    } else if (self.b_variable_timing & 0x20) {
      self.zxn_prescalar      = value;
      self.b_variable_timing &= ~0x20;
    }
  }

  if (((self.group_value[2]    & 0x40) == 0x00) &&
      ((self.b_variable_timing & 0x20) == 0x00)) {
    self.group = NO_GROUP;
  }
}


static void dma_group_3_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[3] & 0x08) {
      self.mask_byte       = value;
      self.group_value[3] &= ~0x08;
    } else if (self.group_value[3] & 0x10) {
      self.match_byte      = value;
      self.group_value[3] &= ~0x10;
    }
  }

  if ((self.group_value[3] & 0x18) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_4_write(u8_t value, int is_first_write) {
  if (is_first_write) {
    self.mode = (value & 0x60) >> 5;
  } else {
    if (self.group_value[4] & 0x04) {
      self.b_starting_address &= 0xFF00;
      self.b_starting_address |= value;
      self.group_value[4]     &= ~0x04;
    } else if (self.group_value[4] & 0x08) {
      self.b_starting_address &= 0x00FF;
      self.b_starting_address |= value << 8;
      self.group_value[4]     &= ~0x08;
    } else if (self.group_value[4] & 0x10) {
      self.interrupt_control = value;
      self.group_value[4]   &= ~0x10;
    } else if (self.interrupt_control & 0x08) {
      self.pulse_control      = value;
      self.interrupt_control &= ~0x08;
    } else if (self.interrupt_control & 0x10) {
      self.interrupt_vector   = value;
      self.interrupt_control &= ~0x10;
    }
  }

  if (((self.group_value[4]    & 0x1C) == 0x00) &&
      ((self.interrupt_control & 0x18) == 0x00)) {
    self.group = NO_GROUP;
  }
}


static void dma_group_5_write(u8_t value, int is_first_write) {
  self.do_restart = value & 0x20;
  self.group      = NO_GROUP;
}


static void dma_group_6_write(u8_t value, int is_first_write) {
  if (is_first_write) {
    switch (value) {
      case E_DMA_CMD_RESET:
        self.is_enabled = 0;
        break;

      case E_DMA_CMD_RESET_PORT_A_TIMING:
      case E_DMA_CMD_RESET_PORT_B_TIMING:
      case E_DMA_CMD_CONTINUE:
      case E_DMA_CMD_DISABLE_INTERRUPTS:
      case E_DMA_CMD_ENABLE_INTERRUPTS:
      case E_DMA_CMD_RESET_AND_DISABLE_INTERRUPTS:
      case E_DMA_CMD_ENABLE_AFTER_RTI:
      case E_DMA_CMD_READ_STATUS_BYTE:
      case E_DMA_CMD_REINITIALISE_STATUS_BYTE:
      case E_DMA_CMD_INITIATE_READ_SEQUENCE:
      case E_DMA_CMD_FORCE_READY:
        break;

      case E_DMA_CMD_ENABLE_DMA:
        self.is_enabled = 1;
        break;

      case E_DMA_CMD_DISABLE_DMA:
        self.is_enabled = 0;
        break;

      case E_DMA_CMD_LOAD:
        dma_load();
        break;

      case E_DMA_CMD_READ_MASK_FOLLOWS:
        break;

      default:
        log_err("dma: unknown command $%02X received\n", value);
        break;
    }

    if (value != E_DMA_CMD_READ_MASK_FOLLOWS) {
      self.group = NO_GROUP;
    }
  } else {
    self.read_mask = value;
    self.group     = NO_GROUP;
  }
}


void dma_write(u16_t address, u8_t value) {
  if ((address & 0x00FF) != 0x6B) {
    log_wrn("dma: Z80 DMA at port $%04X not implemented\n", address);
    return;
  }

  dma_decode_handler_t handlers[N_GROUPS] = {
    dma_group_0_write,
    dma_group_1_write,
    dma_group_2_write,
    dma_group_3_write,
    dma_group_4_write,
    dma_group_5_write,
    dma_group_6_write,
  };
  
  if (self.group == NO_GROUP) {
    self.group = dma_decode_group(value);
    if (self.group != NO_GROUP) {
      self.group_value[self.group] = value;
      handlers[self.group](value, 1);
    }
    return;
  }

  handlers[self.group](value, 0);
}


u8_t dma_read(u16_t address) {
  /* log_wrn("dma: unimplemented DMA read from $%04X\n", address); */

  return 0xFF;
}


static void transfer_one_byte(void) {
  const u8_t byte = self.is_src_io ? io_read(self.src_address) : memory_read(self.src_address);
  self.src_address += self.src_address_delta;
  clock_run(self.src_cycle_length);

  if (self.is_dst_io) {
    io_write(self.dst_address, byte);
  } else {
    memory_write(self.dst_address, byte);
  }
  self.dst_address += self.dst_address_delta;
  clock_run(self.dst_cycle_length);

  self.n_transferred++;

  if (self.zxn_prescalar != 0) {
    /**
     * Each byte should be written at a rate of 875 kHz / prescalar,
     * assuming a 28 Mhz clock (875 kHz = 28 MHz / 32).
     */
    self.next_transfer_ticks = clock_ticks() + self.zxn_prescalar * 32;
  }
}


void dma_run(void) {
  if (!self.is_enabled) {
    return;
  }

  switch (self.mode) {
    case E_MODE_BURST:
      if (self.zxn_prescalar != 0 && clock_ticks() < self.next_transfer_ticks) {
        return;
      }
      transfer_one_byte();
      break;

    case E_MODE_CONTINUOUS:
      while (self.n_transferred < self.block_length) {
        if (self.zxn_prescalar != 0) {
          const u64_t now = clock_ticks();
          if (self.next_transfer_ticks > now) {
            clock_run_28mhz_ticks(self.next_transfer_ticks - now);
          }
        }
        transfer_one_byte();
      }
      break;

    default:
      /* Not implemented in ZXDN. */
      return;
  }

  if (self.n_transferred == self.block_length) {
    if (self.do_restart) {
      self.n_transferred = 0;
    } else {
      self.is_enabled = 0;
    }
  }
}
