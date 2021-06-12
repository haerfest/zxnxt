#include <string.h>
#include "defs.h"
#include "clock.h"
#include "copper.h"
#include "log.h"


#define N_INSTRUCTIONS  1024
#define N_MASK          (2 * N_INSTRUCTIONS - 1)


typedef struct {
  u16_t pc;
  u16_t address;
  u8_t  instruction[N_INSTRUCTIONS * 2];
  u8_t  lsb;
  int   is_lsb;
  int   is_running;
  int   do_wait_for_origin;
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
  self.address = (self.address + 1) & N_MASK;
}


void copper_data_16bit_write(u8_t value)  {
  if (self.is_lsb) {
    self.lsb = value;
  } else {
    const u16_t odd = self.address & (N_MASK - 1);
    self.instruction[odd + 0] = value;
    self.instruction[odd + 1] = self.lsb;
    self.address = (self.address + 1) & N_MASK;
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
      self.do_wait_for_origin = 0;
      self.is_running         = 1;
      break;

    case 2:
      self.do_wait_for_origin = 0;
      self.is_running         = 1;
      break;

    case 3:
      self.pc                 = 0;
      self.do_wait_for_origin = 1;
      self.is_running         = 1;
      break;
  }
}
