#include <strings.h>
#include "defs.h"
#include "log.h"


#define NO_GROUP  0xFF
#define N_GROUPS  7


typedef void (*dma_decode_handler_t)(u8_t value);


typedef struct {
  u8_t* sram;
  u8_t  group;
  u8_t  group_value[N_GROUPS];
  u16_t port_a_starting_address;
  u16_t block_length;
  u8_t  interrupt_control_byte;
  u8_t  read_mask;
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


static void dma_decode_group_0(u8_t value) {
  if (self.group_value[0] & 0x08) {
    self.port_a_starting_address &= 0xF0;
    self.port_a_starting_address |= value;
    self.group_value[0] &= ~0x08;
    return;
  }

  if (self.group_value[0] & 0x10) {
    self.port_a_starting_address &= 0x0F;
    self.port_a_starting_address |= value << 8;
    self.group_value[0] &= ~0x10;
    return;
  }

  if (self.group_value[0] & 0x20) {
    self.block_length &= 0xF0;
    self.block_length |= value;
    self.group_value[0] &= ~0x20;
    return;
  }

  if (self.group_value[0] & 0x40) {
    self.block_length &= 0x0F;
    self.block_length |= value << 8;
    self.group_value[0] &= ~0x40;
    return;
  }

  self.group = NO_GROUP;
}


static void dma_decode_group_1(u8_t value) {
}


static void dma_decode_group_2(u8_t value) {
}


static void dma_decode_group_3(u8_t value) {
}


static void dma_decode_group_4(u8_t value) {
}


static void dma_decode_group_5(u8_t value) {
}


static void dma_first_write_group_6(u8_t value) {
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
}


static void dma_decode_group_6(u8_t value) {
  self.read_mask = value;
  self.group     = NO_GROUP;
}


void dma_write(u16_t address, u8_t value) {
  dma_decode_handler_t handlers[N_GROUPS][2] = {
    { NULL,                    dma_decode_group_0 },
    { NULL,                    dma_decode_group_1 },
    { NULL,                    dma_decode_group_2 },
    { NULL,                    dma_decode_group_3 },
    { NULL,                    dma_decode_group_4 },
    { NULL,                    dma_decode_group_5 },
    { dma_first_write_group_6, dma_decode_group_6 }
  };
  
  if (self.group == NO_GROUP) {
      self.group = dma_decode_group(value);
      if (self.group != NO_GROUP) {
        const dma_decode_handler_t handler = handlers[self.group][0];
        self.group_value[self.group] = value;
        if (handler) {
          handler(value);
        }
      }
      return;
  }

  handlers[self.group][1](value);
}


u8_t dma_read(u16_t address) {
  log_dbg("dma: unimplemented DMA read from $%04X\n", address);

  return 0xFF;
}
