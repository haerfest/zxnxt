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


typedef enum dma_cmd_t {
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


typedef enum dma_mode_t {
  E_MODE_BYTE = 0,
  E_MODE_CONTINUOUS,
  E_MODE_BURST,
  E_MODE_DO_NOT_PROGRAM
} dma_mode_t;


typedef void (*dma_decode_handler_t)(u8_t value, int is_first_write);


typedef struct dma_t {
  u8_t*      sram;
  u8_t       group;
  u8_t       group_value[N_GROUPS];
  u16_t      a_starting_address;
  u16_t      b_starting_address;
  u8_t       a_variable_timing;
  u8_t       b_variable_timing;
  u16_t      block_length;
  u8_t       interrupt_control;
  u8_t       pulse_control;
  u8_t       interrupt_vector;
  u8_t       read_mask;
  u8_t       mask_byte;
  u8_t       match_byte;
  u8_t       zxn_prescalar;
  int        is_enabled;
  u16_t      n_bytes_transferred;
  u16_t      n_blocks_transferred;
  dma_mode_t mode;
  int        do_restart;
  u64_t      next_transfer_ticks;
  u8_t       read_bit;

  int        is_a_to_b;
  u16_t      src_address;
  u16_t      dst_address;
  int        is_src_io;
  int        is_dst_io;
  int        src_address_delta;
  int        dst_address_delta;
  u8_t       src_cycle_length;
  u8_t       dst_cycle_length;
} dma_t;


static dma_t dma;


int dma_init(u8_t* sram) {
  memset(&dma, 0, sizeof(dma));

  dma.sram = sram;

  dma_reset(E_RESET_HARD);

  return 0;
}


void dma_finit(void) {
}


void dma_reset(reset_t reset) {
  dma.group               = NO_GROUP;
  dma.read_mask           = 0x7F;
  dma.is_enabled          = 0;
  dma.zxn_prescalar       = 0;
  dma.do_restart          = 0;
  dma.next_transfer_ticks = 0;
}


static int dma_get_address_delta(int group) {
  switch ((dma.group_value[group] & 0x30) >> 4) {
    case 0:
      return -1;

    case 1:
      return +1;

    default:
      break;
  }

  return 0;
}


static void read_bit_reset(void) {
  int i;

  dma.read_bit = 1;

  for (i = 0; ((dma.read_mask & dma.read_bit) == 0) && (i < 8); i++) {
    dma.read_bit = (dma.read_bit << 1) | (dma.read_bit >> 7);
  }
}


static void read_bit_advance(void) {
  int i;

  for (i = 0; i < 8; i++) {
    dma.read_bit = (dma.read_bit << 1) | (dma.read_bit >> 7);

    if (dma.read_mask & dma.read_bit) {
      break;
    }
  }
}


inline
static void block_reload(void) {
  const int   is_a_io         = dma.group_value[1] & 0x08;
  const int   is_b_io         = dma.group_value[2] & 0x08;
  const int   a_address_delta = dma_get_address_delta(1);
  const int   b_address_delta = dma_get_address_delta(2);
  const u8_t  a_cycle_length  = dma.a_variable_timing & 0x03;
  const u8_t  b_cycle_length  = dma.b_variable_timing & 0x03;

  if (dma.is_a_to_b) {
    /* A -> B */
    dma.src_address       = dma.a_starting_address;
    dma.dst_address       = dma.b_starting_address;
    dma.is_src_io         = is_a_io;
    dma.is_dst_io         = is_b_io;
    dma.src_address_delta = a_address_delta;
    dma.dst_address_delta = b_address_delta;
    dma.src_cycle_length  = a_cycle_length;
    dma.dst_cycle_length  = b_cycle_length;
  } else {
    /* B -> A */
    dma.src_address       = dma.b_starting_address;
    dma.dst_address       = dma.a_starting_address;
    dma.is_src_io         = is_b_io;
    dma.is_dst_io         = is_a_io;
    dma.src_address_delta = b_address_delta;
    dma.dst_address_delta = a_address_delta;
    dma.src_cycle_length  = b_cycle_length;
    dma.dst_cycle_length  = a_cycle_length;
  }
}


static void dma_load(void) {
  block_reload();
  read_bit_reset();

  dma.n_bytes_transferred  = 0;
  dma.n_blocks_transferred = 0;

#if 0
  {
    const char* delta_str[] = {"--", "", "++"};
    const char* mode_str[] = {"BYTE", "CONT", "BRST", "N/A"};

    log_wrn("dma: LOAD %s %s>%s %04X%s>%04X%s %s len=%04X pre=%02X rstrt=%c\n",
            dma.is_a_to_b ? "A>B" : "B>A",
            dma.is_src_io ? "IO" : "M",
            dma.is_dst_io ? "IO" : "M",
            dma.src_address,
            delta_str[1 + dma.src_address_delta],
            dma.dst_address,
            delta_str[1 + dma.dst_address_delta],
            mode_str[dma.mode],
            dma.block_length,
            dma.zxn_prescalar,
            dma.do_restart ? 'Y' : 'N');
  }
#endif
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
  if (is_first_write) {
    dma.is_a_to_b = dma.group_value[0] & 0x04;
  } else {
    if (dma.group_value[0] & 0x08) {
      dma.a_starting_address &= 0xFF00;
      dma.a_starting_address |= value;
      dma.group_value[0]     &= ~0x08;
    } else if (dma.group_value[0] & 0x10) {
      dma.a_starting_address &= 0x00FF;
      dma.a_starting_address |= value << 8;
      dma.group_value[0]     &= ~0x10;
    } else if (dma.group_value[0] & 0x20) {
      dma.block_length &= 0xFF00;
      dma.block_length |= value;
      dma.group_value[0] &= ~0x20;
    } else if (dma.group_value[0] & 0x40) {
      dma.block_length   &= 0x00FF;
      dma.block_length   |= value << 8;
      dma.group_value[0] &= ~0x40;
    }
  }

  if ((dma.group_value[0] & 0x78) == 0x00) {
    dma.group = NO_GROUP;
  }
}


static void dma_group_1_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (dma.group_value[1] & 0x40) {
      dma.a_variable_timing = value;
      dma.group_value[1]   &= ~0x40;
    }
  }

  if ((dma.group_value[1] & 0x40) == 0x00) {
    dma.group = NO_GROUP;
  }
}


static void dma_group_2_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (dma.group_value[2] & 0x40) {
      dma.b_variable_timing = value;
      dma.group_value[2]   &= ~0x40;
    } else if (dma.b_variable_timing & 0x20) {
      dma.zxn_prescalar      = value;
      dma.b_variable_timing &= ~0x20;
    }
  }

  if (((dma.group_value[2]    & 0x40) == 0x00) &&
      ((dma.b_variable_timing & 0x20) == 0x00)) {
    dma.group = NO_GROUP;
  }
}


static void dma_group_3_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (dma.group_value[3] & 0x08) {
      dma.mask_byte       = value;
      dma.group_value[3] &= ~0x08;
    } else if (dma.group_value[3] & 0x10) {
      dma.match_byte      = value;
      dma.group_value[3] &= ~0x10;
    }
  }

  if ((dma.group_value[3] & 0x18) == 0x00) {
    dma.group = NO_GROUP;
  }
}


static void dma_group_4_write(u8_t value, int is_first_write) {
  if (is_first_write) {
    dma.mode = (value & 0x60) >> 5;
  } else {
    if (dma.group_value[4] & 0x04) {
      dma.b_starting_address &= 0xFF00;
      dma.b_starting_address |= value;
      dma.group_value[4]     &= ~0x04;
    } else if (dma.group_value[4] & 0x08) {
      dma.b_starting_address &= 0x00FF;
      dma.b_starting_address |= value << 8;
      dma.group_value[4]     &= ~0x08;
    } else if (dma.group_value[4] & 0x10) {
      dma.interrupt_control = value;
      dma.group_value[4]   &= ~0x10;
    } else if (dma.interrupt_control & 0x08) {
      dma.pulse_control      = value;
      dma.interrupt_control &= ~0x08;
    } else if (dma.interrupt_control & 0x10) {
      dma.interrupt_vector   = value;
      dma.interrupt_control &= ~0x10;
    }
  }

  if (((dma.group_value[4]    & 0x1C) == 0x00) &&
      ((dma.interrupt_control & 0x18) == 0x00)) {
    dma.group = NO_GROUP;
  }
}


static void dma_group_5_write(u8_t value, int is_first_write) {
  dma.do_restart = value & 0x20;
  dma.group      = NO_GROUP;
}


static void dma_group_6_write(u8_t value, int is_first_write) {
  if (is_first_write) {
    switch (value) {
      case E_DMA_CMD_RESET:
        dma.is_enabled = 0;
        break;

      case E_DMA_CMD_RESET_PORT_A_TIMING:
      case E_DMA_CMD_RESET_PORT_B_TIMING:
      case E_DMA_CMD_CONTINUE:
      case E_DMA_CMD_DISABLE_INTERRUPTS:
      case E_DMA_CMD_ENABLE_INTERRUPTS:
      case E_DMA_CMD_RESET_AND_DISABLE_INTERRUPTS:
      case E_DMA_CMD_ENABLE_AFTER_RTI:
      case E_DMA_CMD_FORCE_READY:
        break;

      case E_DMA_CMD_READ_STATUS_BYTE:
      case E_DMA_CMD_REINITIALISE_STATUS_BYTE:
      case E_DMA_CMD_INITIATE_READ_SEQUENCE:
        read_bit_reset();
        break;

      case E_DMA_CMD_ENABLE_DMA:
        dma.is_enabled = 1;
        break;

      case E_DMA_CMD_DISABLE_DMA:
        dma.is_enabled = 0;
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
      dma.group = NO_GROUP;
    }
  } else {
    dma.read_mask = value & 0x7F;
    dma.group     = NO_GROUP;
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
  
  if (dma.group == NO_GROUP) {
    dma.group = dma_decode_group(value);
    if (dma.group != NO_GROUP) {
      dma.group_value[dma.group] = value;
      handlers[dma.group](value, 1);
    }
    return;
  }

  handlers[dma.group](value, 0);
}


u8_t dma_read(u16_t address) {
  u8_t value;

  switch (dma.read_bit) {
    case 0x01:
      /* Status byte. */
      value = 0x1A | ((dma.n_blocks_transferred > 0) << 5) | (dma.n_bytes_transferred > 0);
      break;

    case 0x02:
      /* Byte counter high? */
      value = dma.n_bytes_transferred >> 8;
      break;

    case 0x04:
      /* Byte counter low? */
      value = dma.n_bytes_transferred & 0x00FF;
      break;

    case 0x08:
      /* Port A address low. */
      value = (dma.is_a_to_b ? dma.src_address : dma.dst_address) & 0x00FF;
      break;

    case 0x10:
      /* Port A address high. */
      value = (dma.is_a_to_b ? dma.src_address : dma.dst_address) >> 8;
      break;

    case 0x20:
      /* Port B address low. */
      value = (dma.is_a_to_b ? dma.dst_address : dma.src_address) & 0x00FF;
      break;

    case 0x40:
      /* Port B address high. */
      value = (dma.is_a_to_b ? dma.dst_address : dma.src_address) >> 8;
      break;

    default:
      value = 0x00;
      break;
  }

  read_bit_advance();

  return value;
}


inline
static void transfer_one_byte(void) {
  const u64_t start_ticks = clock_ticks();
  const u8_t  byte        = dma.is_src_io ? io_read(dma.src_address) : memory_read(dma.src_address);
  
  dma.src_address += dma.src_address_delta;
  clock_run(dma.src_cycle_length);

  if (dma.is_dst_io) {
    io_write(dma.dst_address, byte);
  } else {
    memory_write(dma.dst_address, byte);
  }
  dma.dst_address += dma.dst_address_delta;
  clock_run(dma.dst_cycle_length);

  dma.n_bytes_transferred++;

  if (dma.zxn_prescalar != 0) {
    /**
     * Each byte should be written at a rate of 875 kHz / prescalar,
     * assuming a 28 Mhz clock (875 kHz = 28 MHz / 32).
     */
    dma.next_transfer_ticks = start_ticks + dma.zxn_prescalar * 32;
  }
}


inline
static void dma_run(void) {
  u64_t now;

  if (!dma.is_enabled) {
    return;
  }

  switch (dma.mode) {
    case E_MODE_BURST:
      if (dma.zxn_prescalar != 0 && clock_ticks() < dma.next_transfer_ticks) {
        return;
      }
      transfer_one_byte();
      break;

    case E_MODE_CONTINUOUS:
      while (dma.n_bytes_transferred < dma.block_length) {
        transfer_one_byte();
        if (dma.zxn_prescalar != 0) {
          now = clock_ticks();
          if (dma.next_transfer_ticks > now) {
            clock_run_28mhz_ticks(dma.next_transfer_ticks - now);
          }
        }
      }
      break;

    default:
      /* Not implemented in ZXDN. */
      return;
  }

  if (dma.n_bytes_transferred == dma.block_length) {
    if (dma.do_restart) {
      block_reload();
      dma.n_blocks_transferred++;
      dma.n_bytes_transferred = 0;
    } else {
      dma.is_enabled = 0;
    }
  }
}
