#include <stdio.h>
#include "defs.h"
#include "sdcard.h"


/**
 * See:
 * - http://elm-chan.org/docs/mmc/mmc_e.html.
 * - SD Specifiations Part 1 Physical Layer Simplified Specification Version 6.00
 */


#define N_SDCARDS 2


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
  E_STATE_INACTIVE = 0,
  E_STATE_IDLE,
  E_STATE_READY,
  E_STATE_IDENTIFICATION,
  E_STATE_STANDBY,
  E_STATE_TRANSFER,
  E_STATE_SENDING_DATA,
  E_STATE_RECEIVE_DATA,
  E_STATE_PROGRAMMING,
  E_STATE_DISCONNECT
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
  int        in_app_cmd;
} sdcard_t;


static sdcard_t self[N_SDCARDS];


int sdcard_init(void) {
  int n;

  for (n = 0; n < N_SDCARDS; n++) {
    self[n].state           = E_STATE_IDLE;
    self[n].error           = E_ERROR_NONE;
    self[n].command_length  = 0;
    self[n].response_length = 0;
    self[n].response_index  = 0;
    self[n].block_length    = 512;
    self[n].in_app_cmd      = 0;
  }

  return 0;
}


void sdcard_finit(void) {
}


u8_t sdcard_read(sdcard_nr_t n, u16_t address) {
  if (self[n].response_index < self[n].response_length) {
    return self[n].response_buffer[self[n].response_index++];
  }

  /* Standard R1 response. */
  return self[n].error | (self[n].state == E_STATE_IDLE);
}


static void sdcard_handle_command(sdcard_nr_t n) {
  const u8_t command = self[n].command_buffer[0] & 0x3F;
  int        i;

  fprintf(stderr, "sdcard: received CMD%u\n", command);

  /* Defaults: no error, R1 response. */
  self[n].error           = E_ERROR_NONE;
  self[n].response_length = 0;
  self[n].response_index  = 0;

  switch (command) {
    case E_CMD_GO_IDLE_STATE:
      self[n].state = E_STATE_IDLE;
      return;

    case E_CMD_SEND_OP_COND:
      self[n].state              = E_STATE_IDLE;
      self[n].response_buffer[0] = 0x00;  /* Need to clear idle bit. */
      self[n].response_length    = 1;
      return;
 
    case E_CMD_SEND_IF_COND:
      self[n].state = E_STATE_IDLE;
      self[n].error = E_ERROR_ILLEGAL_COMMAND;
      return;

    case E_CMD_SEND_CSD:
      self[n].state = E_STATE_IDLE;
      self[n].response_buffer[0] = 0x00;
      for (i = 1; i <= 16; i++) {
        self[n].response_buffer[i] = 0x00;
      }
      self[n].response_buffer[17] = 0x00;  /* CRC */
      self[n].response_buffer[18] = 0x00;
      self[n].response_length     = 19;
      return;
      
    case E_CMD_STOP_TRANSMISSION:
      self[n].state              = E_STATE_IDLE;
      self[n].response_buffer[0] = 0x01;
      self[n].response_buffer[1] = 0xFF;  /* No longer busy. */
      self[n].response_length    = 2;
      return;

    case E_CMD_SET_BLOCKLEN:
      self[n].block_length = self[n].command_buffer[1] << 24 | self[n].command_buffer[2] << 16 | self[n].command_buffer[3] << 8 | self[n].command_buffer[4];
      fprintf(stderr, "sdcard: block length set to %u bytes\n", self[n].block_length);
      return;

    case E_CMD_APP_SEND_OP_COND:
      if (self[n].in_app_cmd) {
        self[n].in_app_cmd         = 0;
        self[n].state              = E_STATE_IDLE;
        self[n].response_buffer[0] = 0x00;  /* Need to indicate busy. */
        self[n].response_length    = 1;
        return;
      }
      break;
 
    case E_CMD_APP_CMD:
      self[n].in_app_cmd = 1;
      return;

    case E_CMD_READ_OCR:
      self[n].state              = E_STATE_IDLE;
      self[n].response_buffer[0] = 0;
      self[n].response_buffer[1] = 0;
      self[n].response_buffer[2] = 0;
      self[n].response_buffer[3] = 0;
      self[n].response_length    = 4;
      return;

    default:
      break;
  }

  fprintf(stderr, "sdcard: received unimplemented command CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", command, self[n].command_buffer[0], self[n].command_buffer[1], self[n].command_buffer[2], self[n].command_buffer[3], self[n].command_buffer[4], self[n].command_buffer[5]);
  self[n].state      = E_STATE_IDLE;
  self[n].error      = E_ERROR_ILLEGAL_COMMAND;
  self[n].in_app_cmd = 0;
}


void sdcard_write(sdcard_nr_t n, u16_t address, u8_t value) {
  if (self[n].command_length == sizeof(self[n].command_buffer)) {
    self[n].command_length = 0;
  }

  if (self[n].command_length == 0) {
    if (value >> 6 != 0x01) {
      fprintf(stderr, "sdcard: invalid first byte $%02X of command\n", value);
      return;
    }
  } else if (self[n].command_length == sizeof(self[n].command_buffer) - 1) {
    if ((value & 0x01) != 0x01) {
      fprintf(stderr, "sdcard: invalid last byte $%02X of command\n", value);
      self[n].command_length = 0;
      return;
    }
  }

  self[n].command_buffer[self[n].command_length] = value;
  self[n].command_length++;

  if (self[n].command_length == sizeof(self[n].command_buffer)) {
    sdcard_handle_command(n);
  }
}
