#include <stdio.h>
#include "defs.h"


/* See http://elm-chan.org/docs/mmc/mmc_e.html. */


typedef enum {
  E_CMD_GO_IDLE_STATE     = 0,
  E_CMD_SEND_OP_COND      = 1,
  E_CMD_SEND_IF_COND      = 8,
  E_CMD_SEND_CSD          = 9,
  E_CMD_STOP_TRANSMISSION = 12,
  E_CMD_SET_BLOCKLEN      = 16,
  E_CMD_APP_SEND_OP_COND  = 41,  /* Prepended by E_CMD_APP_CMD */
  E_CMD_APP_CMD           = 55,
  E_CMD_READ_OCR          = 58
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
  E_STATE_INITIALISED,
  E_STATE_BUSY,
  E_STATE_RESPONDING,
  E_STATE_APP_CMD
} state_t;

  
typedef struct {
  state_t    state;
  u8_t       command_buffer[6];
  int        command_length;
  u8_t       response_buffer[1 + 16 + 2];  /* R1 + payload + CRC. */
  int        response_length;
  int        response_index;
  u8_t       error;
  u32_t      block_length;
} sdcard_t;


static sdcard_t self;


int sdcard_init(void) {
  self.state           = E_STATE_IDLE;
  self.error           = E_ERROR_NONE;
  self.command_length  = 0;
  self.response_length = 0;
  self.response_index  = 0;
  self.block_length    = 512;
  return 0;
}


void sdcard_finit(void) {
}


u8_t sdcard_read(u16_t address) {
  fprintf(stderr, "sdcard: read\n");
  if (self.response_index < self.response_length) {
    return self.response_buffer[self.response_index++];
  }

  /* Standard R1 response. */
  return self.error | (self.state == E_STATE_IDLE);
}


static void sdcard_handle_command(void) {
  const u8_t command = self.command_buffer[0] & 0x3F;
  int        i;

  fprintf(stderr, "sdcard: received CMD%u\n", command);

  /* Defaults: no error, R1 response. */
  self.error           = E_ERROR_NONE;
  self.response_length = 0;
  self.response_index  = 0;

  switch (command) {
    case E_CMD_GO_IDLE_STATE:
      self.state = E_STATE_IDLE;
      return;

    case E_CMD_SEND_OP_COND:
      self.state              = E_STATE_IDLE;
      self.response_buffer[0] = 0x00;  /* Need to clear idle bit. */
      self.response_length    = 1;
      return;
 
    case E_CMD_SEND_IF_COND:
      self.state = E_STATE_IDLE;
      self.error = E_ERROR_ILLEGAL_COMMAND;
      return;

    case E_CMD_SEND_CSD:
      self.state = E_STATE_IDLE;
      self.response_buffer[0] = 0x00;
      for (i = 1; i <= 16; i++) {
        self.response_buffer[i] = 0x00;
      }
      self.response_buffer[17] = 0x00;  /* CRC */
      self.response_buffer[18] = 0x00;
      self.response_length     = 19;
      return;
      
    case E_CMD_STOP_TRANSMISSION:
      self.state              = E_STATE_IDLE;
      self.response_buffer[0] = 0x01;
      self.response_buffer[1] = 0xFF;  /* No longer busy. */
      self.response_length    = 2;
      return;

    case E_CMD_SET_BLOCKLEN:
      self.block_length = self.command_buffer[1] << 24 | self.command_buffer[2] << 16 | self.command_buffer[3] << 8 | self.command_buffer[4];
      fprintf(stderr, "sdcard: block length set to %u bytes\n", self.block_length);
      return;

    case E_CMD_APP_SEND_OP_COND:
      if (self.state == E_STATE_APP_CMD) {
        self.state              = E_STATE_IDLE;
        self.response_buffer[0] = 0x00;  /* Need to indicate busy. */
        self.response_length    = 1;
        return;
      }
      break;
 
    case E_CMD_APP_CMD:
      self.state = E_STATE_APP_CMD;  /* Expect a further CMD. */
      return;

    case E_CMD_READ_OCR:
      self.state              = E_STATE_IDLE;
      self.response_buffer[0] = 0;
      self.response_buffer[1] = 0;
      self.response_buffer[2] = 0;
      self.response_buffer[3] = 0;
      self.response_length    = 4;
      return;

    default:
      break;
  }

  fprintf(stderr, "sdcard: received unimplemented command CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", command, self.command_buffer[0], self.command_buffer[1], self.command_buffer[2], self.command_buffer[3], self.command_buffer[4], self.command_buffer[5]);
  self.state = E_STATE_IDLE;
  self.error = E_ERROR_ILLEGAL_COMMAND;
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
