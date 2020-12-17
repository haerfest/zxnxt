#include <stdio.h>
#include "defs.h"


/* See http://elm-chan.org/docs/mmc/mmc_e.html. */


typedef enum {
  E_CMD_GO_IDLE_STATE     = 0,
  E_CMD_SEND_IF_COND      = 8,
  E_CMD_STOP_TRANSMISSION = 12
} cmd_t;


typedef enum {
  E_RESPONSE_R1,
  E_RESPONSE_R1B_R1,
  E_RESPONSE_R1B_BUSY
} response_t;


typedef enum {
  E_ERROR_NONE            = 0x00,
  E_ERROR_ILLEGAL_COMMAND = 0x04
} error_t;

typedef enum {
  E_STATE_IDLE = 0,
  E_STATE_BUSY,
  E_STATE_RESPONDING,
} state_t;

  
typedef struct {
  state_t    state;
  u8_t       command_buffer[6];
  int        command_length;
  response_t response;
  u8_t       error;
} sdcard_t;


static sdcard_t self;


int sdcard_init(void) {
  self.state          = E_STATE_IDLE;
  self.response       = E_RESPONSE_R1;
  self.error          = E_ERROR_NONE;
  self.command_length = 0;
  return 0;
}


void sdcard_finit(void) {
}


u8_t sdcard_read(u16_t address) {
  switch (self.state) {
    case E_STATE_IDLE:
      return self.error | 0x01;

    case E_STATE_BUSY:
      return 0x00;
 
    case E_STATE_RESPONDING:
      switch (self.response) {
        case E_RESPONSE_R1:
          self.state = E_STATE_IDLE;
          return self.error;

        case E_RESPONSE_R1B_R1:
          self.response = E_RESPONSE_R1B_BUSY;
          return self.error;

        case E_RESPONSE_R1B_BUSY:
          self.state    = E_STATE_IDLE;
          self.response = E_RESPONSE_R1;
          return 0xFF;
      }
      break;
  }
}


static void sdcard_handle_command(void) {
  const u8_t command = self.command_buffer[0] & 0x3F;

  switch (command) {
    case E_CMD_GO_IDLE_STATE:
      self.state    = E_STATE_IDLE;
      self.error    = E_ERROR_NONE;
      self.response = E_RESPONSE_R1;
      break;

    case E_CMD_SEND_IF_COND:
      self.state    = E_STATE_IDLE;
      self.error    = E_ERROR_ILLEGAL_COMMAND;
      self.response = E_RESPONSE_R1;
      break;
 
    case E_CMD_STOP_TRANSMISSION:
      self.state    = E_STATE_RESPONDING;
      self.error    = E_ERROR_NONE;
      self.response = E_RESPONSE_R1B_R1;
      break;

    default:
      fprintf(stderr, "sdcard: received unimplemented command CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", command, self.command_buffer[0], self.command_buffer[1], self.command_buffer[2], self.command_buffer[3], self.command_buffer[4], self.command_buffer[5]);
      self.state    = E_STATE_IDLE;
      self.error    = E_ERROR_ILLEGAL_COMMAND;
      self.response = E_RESPONSE_R1;
      break;
  }
}


void sdcard_write(u16_t address, u8_t value) {
  if (self.command_length == sizeof(self.command_buffer)) {
    self.command_length = 0;
  }

  if (self.command_length == 0) {
    if (value >> 6 != 0x01) {
      fprintf(stderr, "sdcard: invalid first byte $%02X of command\n", value);
      return;
    }
  } else if (self.command_length == sizeof(self.command_buffer) - 1) {
    if ((value & 0x01) != 0x01) {
      fprintf(stderr, "sdcard: invalid last byte $%02X of command\n", value);
      self.command_length = 0;
      return;
    }
  }

  self.command_buffer[self.command_length] = value;
  self.command_length++;

  if (self.command_length == sizeof(self.command_buffer)) {
    sdcard_handle_command();
  }
}
