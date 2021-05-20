#include <strings.h>
#include "defs.h"
#include "log.h"


#define NO_GROUP  0xFF
#define N_GROUPS  7


typedef void (*dma_decode_handler_t)(u8_t value, int is_first_write);


typedef struct {
  u8_t* sram;
  u8_t  group;
  u8_t  group_value[N_GROUPS];
  u16_t port_a_starting_address;
  u16_t port_b_starting_address;
  u8_t  port_a_variable_timing;
  u8_t  port_b_variable_timing;
  u16_t block_length;
  u8_t  interrupt_control;
  u8_t  pulse_control;
  u8_t  interrupt_vector;
  u8_t  read_mask;
  u8_t  mask_byte;
  u8_t  match_byte;
} dma_t;


static dma_t self;


int dma_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.sram  = sram;
  self.group = NO_GROUP;

  return 0;
}


void dma_finit(void) {
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
      self.port_a_starting_address &= 0xF0;
      self.port_a_starting_address |= value;
      self.group_value[0] &= ~0x08;
    } else if (self.group_value[0] & 0x10) {
      self.port_a_starting_address &= 0x0F;
      self.port_a_starting_address |= value << 8;
      self.group_value[0] &= ~0x10;
    } else if (self.group_value[0] & 0x20) {
      self.block_length &= 0xF0;
      self.block_length |= value;
      self.group_value[0] &= ~0x20;
    } else if (self.group_value[0] & 0x40) {
      self.block_length &= 0x0F;
      self.block_length |= value << 8;
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
      self.port_a_variable_timing = value;
      self.group_value[1] &= ~0x40;
    }
  }

  if ((self.group_value[1] & 0x40) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_2_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[2] & 0x40) {
      self.port_b_variable_timing = value;
      self.group_value[2] &= ~0x40;
    }
  }

  if ((self.group_value[2] & 0x40) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_3_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[3] & 0x08) {
      self.mask_byte = value;
      self.group_value[3] &= ~0x08;
    } else if (self.group_value[3] & 0x10) {
      self.match_byte = value;
      self.group_value[3] &= ~0x10;
    }
  }

  if ((self.group_value[3] & 0x18) == 0x00) {
    self.group = NO_GROUP;
  }
}


static void dma_group_4_write(u8_t value, int is_first_write) {
  if (!is_first_write) {
    if (self.group_value[4] & 0x04) {
      self.port_b_starting_address &= 0xF0;
      self.port_b_starting_address |= value;
      self.group_value[4] &= ~0x04;
    } else if (self.group_value[4] & 0x08) {
      self.port_b_starting_address &= 0x0F;
      self.port_b_starting_address |= value << 8;
      self.group_value[4] &= ~0x08;
    } else if (self.group_value[4] & 0x10) {
      self.interrupt_control = value;
      self.group_value[4] &= ~0x10;
    } else if (self.interrupt_control & 0x08) {
      self.pulse_control = value;
      self.interrupt_control &= ~0x08;
    } else if (self.interrupt_control & 0x10) {
      self.interrupt_vector = value;
      self.interrupt_control &= ~0x10;
    }
  }

  if (((self.group_value[4]    & 0x78) == 0x00) &&
      ((self.interrupt_control & 0x18) == 0x00)) {
    self.group = NO_GROUP;
  }
}


static void dma_group_5_write(u8_t value, int is_first_write) {
  self.group = NO_GROUP;
}


static void dma_group_6_write(u8_t value, int is_first_write) {
  if (is_first_write) {
    switch (value) {
      case 0xC3:
      case 0xC7:
      case 0xCB:
      case 0xCF:
      case 0xD3:
      case 0xAF:
      case 0xAB:
      case 0xA3:
      case 0xB7:
      case 0xBF:
      case 0x8B:
      case 0xA7:
      case 0xB3:
      case 0x87:
      case 0x83:
        self.group = NO_GROUP;
        break;

      case 0xBB:
        /* Read mask will follow. */
        break;

      default:
        self.group = NO_GROUP;
        break;
    }
  } else {
    self.read_mask = value;
    self.group     = NO_GROUP;
  }
}


void dma_write(u16_t address, u8_t value) {
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
  log_dbg("dma: unimplemented DMA read from $%04X\n", address);

  return 0xFF;
}
