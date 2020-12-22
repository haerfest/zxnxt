#include <stdio.h>
#include "defs.h"
#include "sdcard.h"


/**
 * See:
 * - http://elm-chan.org/docs/mmc/mmc_e.html.
 * - SD Specifiations Part 1 Physical Layer Simplified Specification Version 6.00
 */


#define N_SDCARDS                 2
#define MAX_RESPONSE_PAYLOAD_SIZE 512
#define SDCARD_IMAGE              "tbblue.mmc"


typedef enum {
  E_CCS_SDSC = 0,
  E_CCS_SDHC = 1,
  E_CCS_SDXC = E_CCS_SDHC
} ccs_t;

typedef enum {
  E_CMD_GO_IDLE_STATE     = 0,
  E_CMD_SEND_OP_COND      = 1,
  E_CMD_SEND_IF_COND      = 8,
  E_CMD_SEND_CSD          = 9,
  E_CMD_STOP_TRANSMISSION = 12,
  E_CMD_SET_BLOCKLEN      = 16,
  E_CMD_READ_SINGLE_BLOCK = 17,
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
  u8_t       response_buffer[1 + MAX_RESPONSE_PAYLOAD_SIZE + 2];  /* R1 + payload + CRC. */
  int        response_length;
  int        response_index;
  u8_t       error;
  u32_t      block_length;
  int        in_app_cmd;
  FILE*      fp;
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
    self[n].fp              = NULL;

    if (n == 0) {
      self[n].fp = fopen(SDCARD_IMAGE, "rb");
      if (self[n].fp == NULL) {
        fprintf(stderr, "sdcard%d: could not open %s for reading\n", n, SDCARD_IMAGE);
        for (n--; n >= 0; n--) {
          fclose(self[n].fp);
        }
        return -1;
      }
    }
  }

  return 0;
}


void sdcard_finit(void) {
  int n;

  for (n = 0; n < N_SDCARDS; n++) {
    if (self[n].fp != NULL) {
      fclose(self[n].fp);
      self[n].fp = NULL;
    }
  }
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

  fprintf(stderr, "sdcard%d: received CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", n, command, self[n].command_buffer[0], self[n].command_buffer[1], self[n].command_buffer[2], self[n].command_buffer[3], self[n].command_buffer[4], self[n].command_buffer[5]);

  if (self[n].fp != NULL) {
    u32_t      block_length;
    long       block;
    int        i;

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
        self[n].response_buffer[0] = 0x00;   /* R1. */
        self[n].response_buffer[1] = 0xFE;   /* Start block token. */
        for (i = 0; i < 16; i++) {           /* CSD. */
          self[n].response_buffer[2 + i] = 0x00;
        }
        self[n].response_buffer[2 + 16 + 0] = 0x00;  /* CRC. */
        self[n].response_buffer[2 + 16 + 1] = 0x00;
        self[n].response_length             = 2 + 16 + 2;
        return;
      
      case E_CMD_STOP_TRANSMISSION:
        self[n].state              = E_STATE_TRANSFER;
        self[n].response_buffer[0] = 0x01;
        self[n].response_buffer[1] = 0xFF;  /* No longer busy. */
        self[n].response_length    = 2;
        return;

      case E_CMD_SET_BLOCKLEN:
        block_length = self[n].command_buffer[1] << 24
                     | self[n].command_buffer[2] << 16
                     | self[n].command_buffer[3] << 8
                     | self[n].command_buffer[4];
        if (block_length > MAX_RESPONSE_PAYLOAD_SIZE) {
          fprintf(stderr, "sdcard%d: unsupported block length %u bytes\n", n, block_length);
          break;
        }
        self[n].block_length = block_length;
        return;

      case E_CMD_READ_SINGLE_BLOCK:
        block = self[n].command_buffer[1] << 24 | self[n].command_buffer[2] << 16 | self[n].command_buffer[3] << 8 | self[n].command_buffer[4];

        fprintf(stderr, "sdcard%d: reading %u bytes from block %ld in %s\n", n, self[n].block_length, block, SDCARD_IMAGE);

        if (fseek(self[n].fp, block * self[n].block_length, SEEK_SET) != 0) {
          fprintf(stderr, "sdcard%d: error seeking to block %ld in %s\n", n, block, SDCARD_IMAGE);
          return;
        }

        if (fread(&self[n].response_buffer[2], self[n].block_length, 1, self[n].fp) != 1) {
          fprintf(stderr, "sdcard%d: error reading %u bytes from block %ld in %s\n", n, self[n].block_length, block, SDCARD_IMAGE);
          return;
        }

        self[n].response_buffer[0]                            = 0x00;  /* R1. */
        self[n].response_buffer[1]                            = 0xFE;  /* Start block token. */
        self[n].response_buffer[2 + self[n].block_length + 0] = 0x00;  /* CRC. */
        self[n].response_buffer[2 + self[n].block_length + 1] = 0x00;
        self[n].response_length                               = 2 + self[n].block_length + 2;
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
        self[n].response_buffer[0] = 0x00;                    /* R1. */
        self[n].response_buffer[1] = 0x80 | E_CCS_SDXC << 6;  /* Powered-up + CCS. */
        self[n].response_buffer[2] = 0;
        self[n].response_buffer[3] = 0;
        self[n].response_buffer[4] = 0;
        self[n].response_length    = 5;
        return;

      default:
        break;
    }
  }

  fprintf(stderr, "sdcard%d: signalling error\n", n);
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
      fprintf(stderr, "sdcard%d: invalid first byte $%02X of command\n", n, value);
      return;
    }
  } else if (self[n].command_length == sizeof(self[n].command_buffer) - 1) {
    if ((value & 0x01) != 0x01) {
      fprintf(stderr, "sdcard%d: invalid last byte $%02X of command\n", n, value);
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
