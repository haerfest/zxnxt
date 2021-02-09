#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "log.h"
#include "sdcard.h"


/**
 * See:
 * - http://elm-chan.org/docs/mmc/mmc_e.html.
 * - SD Specifiations Part 1 Physical Layer Simplified Specification Version 6.00
 */


#define SDSC_MAX_SIZE     (2L * 1024 * 1024 * 1024 - 1)
#define N_SDCARDS         2
#define MAX_BLOCK_LENGTH  1024
#define SDCARD_IMAGE      "tbblue.mmc"

#define TOKEN_BUSY        0x00
#define TOKEN_NOT_BUSY    0xAA  /* Anything other than TOKEN_BUSY or TOKEN_NO_DATA. */
#define TOKEN_START_DATA  0xFE
#define TOKEN_NO_DATA     0xFF


typedef enum {
  E_CMD_GO_IDLE_STATE       = 0,
  E_CMD_SEND_OP_COND        = 1,
  E_CMD_SEND_IF_COND        = 8,
  E_CMD_SEND_CSD            = 9,
  E_CMD_STOP_TRANSMISSION   = 12,
  E_CMD_SEND_STATUS         = 13,
  E_CMD_SET_BLOCKLEN        = 16,
  E_CMD_READ_SINGLE_BLOCK   = 17,
  E_CMD_READ_MULTIPLE_BLOCK = 18,
  E_CMD_WRITE_SINGLE_BLOCK  = 24,
  E_CMD_APP_SEND_OP_COND    = 41,  /* Prepended by E_CMD_APP_CMD */
  E_CMD_APP_CMD             = 55,
  E_CMD_READ_OCR            = 58
} cmd_t;

typedef enum {
  E_RESPONSE_R1,
  E_RESPONSE_R1B_R1,
  E_RESPONSE_R1B_BUSY
} response_t;

typedef enum {
  E_ERROR_NONE            = 0x00,
  E_ERROR_ILLEGAL_COMMAND = 0x04,
  E_ERROR_PARAMETER_ERROR = 0x40
} error_t;

typedef enum {
  E_STATE_IDLE = 0,
  E_STATE_READY,
  E_STATE_IDENTIFICATION,
  E_STATE_STANDBY,
  E_STATE_TRANSFER,
  E_STATE_SENDING_DATA,
  E_STATE_RECEIVE_DATA,
  E_STATE_PROGRAMMING,
  E_STATE_DISCONNECT,
  E_STATE_INACTIVE
} state_t;
  
typedef struct {
  state_t    state;
  u8_t       command_buffer[6];
  int        command_length;
  u8_t       command;
  u8_t       response_buffer[1 + 1 + MAX_BLOCK_LENGTH + 2];  /* R1 + start data token + block + CRC. */
  int        response_length;
  int        response_index;
  u8_t       data_buffer[MAX_BLOCK_LENGTH + 2];  /* block + CRC. */
  int        data_index;
  u8_t       error;
  u32_t      block_length;
  u32_t      position;
  int        in_app_cmd;
  FILE*      fp;
  long       size;
  int        is_sdsc;
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
    self[n].position        = 0;
    self[n].in_app_cmd      = 0;
    self[n].fp              = NULL;
    self[n].size            = 0;
    self[n].is_sdsc         = 1;

    if (n == 0) {
      self[n].fp = fopen(SDCARD_IMAGE, "r+");
      if (self[n].fp == NULL) {
        log_err("sdcard%d: could not open %s for reading and writing\n", n, SDCARD_IMAGE);
        for (n--; n >= 0; n--) {
          fclose(self[n].fp);
        }
        return -1;
      }

      if (fseek(self[n].fp, 0L, SEEK_END) != 0) {
        log_err("sdcard%d: error seeking to the end in %s\n", n, SDCARD_IMAGE);
        for (n--; n >= 0; n--) {
          fclose(self[n].fp);
        }
        return -1;
      }

      self[n].size    = ftell(self[n].fp);
      self[n].is_sdsc = self[n].size <= SDSC_MAX_SIZE;
      if (self[n].is_sdsc) {
        self[n].block_length = self[n].size == SDSC_MAX_SIZE ? 1024 : 512;
      }

      log_dbg("sdcard%d: %s capacity is %ld bytes: %s, block length %u\n", n, SDCARD_IMAGE, self[n].size, self[n].is_sdsc ? "SDSC" : "SDHC/SDXC", self[n].block_length);
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


static int sdcard_block_read(sdcard_nr_t n, u8_t* response_buffer) {
  log_dbg("sdcard%d: reading %u bytes from %s\n", n, self[n].block_length, SDCARD_IMAGE);

  if (fread(&response_buffer[1], self[n].block_length, 1, self[n].fp) != 1) {
    log_err("sdcard%d: error reading %u bytes from %s\n", n, self[n].block_length, SDCARD_IMAGE);
    response_buffer[0] = 0x03;  /* Data error token (card controller error) . */
    return 1;
  }

  response_buffer[0]                            = TOKEN_START_DATA;
  response_buffer[1 + self[n].block_length + 0] = 0x00;  /* CRC. */
  response_buffer[1 + self[n].block_length + 1] = 0x00;

  return 1 + self[n].block_length + 2;
}


u8_t sdcard_read(sdcard_nr_t n, u16_t address) {
  u8_t response;

  /* If there is no response left, should we request one on the fly? */
  if (self[n].fp != NULL) {
    if (self[n].response_index == self[n].response_length) {
      if (self[n].command == E_CMD_READ_MULTIPLE_BLOCK) {
        self[n].position += self[n].block_length;
        self[n].response_length = sdcard_block_read(n, self[n].response_buffer);
        self[n].response_index  = 0;
      }
    }
  }

  /* Is there a response available? */
  if (self[n].response_index < self[n].response_length) {
    return self[n].response_buffer[self[n].response_index++];
  }
  
  /* Standard R1 response. */
  response = self[n].error | (self[n].state == E_STATE_IDLE);

  /* Error bits are cleard by the host when read. */
  self[n].error = E_ERROR_NONE;

  return response;
}


static void sdcard_fill_csd(sdcard_nr_t n, u8_t* csd) {
  memset(csd, 0x00, 16);

  if (self[n].is_sdsc) {
    /* CSD Version 1.0: set C_SIZE, C_SIZE_MULT, and READ_BL_LEN. */
    const u32_t n_blocks = self[n].size / self[n].block_length;
    u32_t       c_size;

    /* Invariant: multi = 2 ^ multi_log2. */    
    u8_t        multi_log2   = 1;
    u16_t       multi        = 2;

    do {
      multi_log2++;
      multi *= 2;
      c_size = n_blocks / multi - 1;
    } while (c_size > 4095);

    log_dbg("sdcard%d: block_length=%u multi=%u c_size=%u\n", n, self[n].block_length, multi, c_size);

    csd[ 5] = (self[n].block_length == 1024 ? 10 : 9);
    csd[ 6] = (c_size >> 10) & 0x03;
    csd[ 7] = (c_size >>  2) & 0xFF;
    csd[ 8] = (c_size & 0x03) << 6;
    csd[ 9] = ((multi_log2 - 2) >> 1) & 0x03;
    csd[10] = ((multi_log2 - 2) & 0x01) << 7;
  } else {
    /* CSD Version 2.0: only set C_SIZE. */
    const u32_t c_size = self[n].size / (512 * 1024) - 1;
    csd[7] = (c_size >> 16) & 0x3F;
    csd[8] = (c_size >>  8) & 0xFF;
    csd[9] = (c_size >>  0) & 0xFF;
  }
}


static void sdcard_handle_command(sdcard_nr_t n) {
  log_dbg("sdcard%d: received CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", n, self[n].command, self[n].command_buffer[0], self[n].command_buffer[1], self[n].command_buffer[2], self[n].command_buffer[3], self[n].command_buffer[4], self[n].command_buffer[5]);

  if (self[n].fp != NULL) {
    u32_t block_length;

    /* Defaults: no error, R1 response. */
    self[n].error           = E_ERROR_NONE;
    self[n].response_length = 0;
    self[n].response_index  = 0;

    switch (self[n].command) {
      case E_CMD_GO_IDLE_STATE:
        self[n].state = E_STATE_IDLE;
        return;

      case E_CMD_SEND_OP_COND:
        self[n].response_buffer[0] = 0x00;  /* Need to clear idle bit. */
        self[n].response_length    = 1;
        return;
 
      case E_CMD_SEND_IF_COND:
        self[n].error = E_ERROR_ILLEGAL_COMMAND;
        return;

      case E_CMD_SEND_CSD:
        self[n].response_buffer[0] = 0x00;   /* R1. */
        self[n].response_buffer[1] = TOKEN_START_DATA;
        
        sdcard_fill_csd(n, &self[n].response_buffer[2]);

        self[n].response_buffer[2 + 16 + 0] = 0x00;  /* CRC. */
        self[n].response_buffer[2 + 16 + 1] = 0x00;
        self[n].response_length             = 2 + 16 + 2;
        return;
      
      case E_CMD_STOP_TRANSMISSION:
        self[n].state              = E_STATE_TRANSFER;
        self[n].response_buffer[0] = 0x01;
        self[n].response_buffer[1] = TOKEN_NOT_BUSY;
        self[n].response_length    = 2;
        return;

      case E_CMD_SEND_STATUS:
        self[n].response_buffer[0] = self[n].error | (self[n].state == E_STATE_IDLE);
        self[n].response_buffer[1] = (self[n].error != E_ERROR_NONE) << 2;
        self[n].response_length    = 2;
        return;

      case E_CMD_SET_BLOCKLEN:
        if (self[n].is_sdsc) {
          block_length = self[n].command_buffer[1] << 24
                       | self[n].command_buffer[2] << 16
                       | self[n].command_buffer[3] << 8
                       | self[n].command_buffer[4];
          if (block_length != 512 && block_length != 1024) {
            log_wrn("sdcard%d: unsupported block length %u bytes\n", n, block_length);
            break;
          }
          self[n].block_length = block_length;
          return;
        }
        break;

      case E_CMD_READ_SINGLE_BLOCK:
      case E_CMD_READ_MULTIPLE_BLOCK:
        self[n].position = self[n].command_buffer[1] << 24
                         | self[n].command_buffer[2] << 16
                         | self[n].command_buffer[3] << 8
                         | self[n].command_buffer[4];

        /* SDHC/SDXC addresses are block numbers. */
        if (!self[n].is_sdsc) {
          self[n].position *= self[n].block_length;
        }

        if (fseek(self[n].fp, (long) self[n].position, SEEK_SET) != 0) {
          log_err("sdcard%d: error seeking to position %u in %s\n", n, self[n].position, SDCARD_IMAGE);
          self[n].error = E_ERROR_PARAMETER_ERROR;
          return;
        }

        self[n].state              = E_STATE_SENDING_DATA;
        self[n].response_buffer[0] = 0x00;  /* R1. */
        self[n].response_length    = 1 + sdcard_block_read(n, &self[n].response_buffer[1]);
        return;

      case E_CMD_WRITE_SINGLE_BLOCK:
        self[n].position = self[n].command_buffer[1] << 24
                         | self[n].command_buffer[2] << 16
                         | self[n].command_buffer[3] << 8
                         | self[n].command_buffer[4];

        /* SDHC/SDXC addresses are block numbers. */
        if (!self[n].is_sdsc) {
          self[n].position *= self[n].block_length;
        }

        if (fseek(self[n].fp, (long) self[n].position, SEEK_SET) != 0) {
          log_err("sdcard%d: error seeking to position %u in %s\n", n, self[n].position, SDCARD_IMAGE);
          self[n].error = E_ERROR_PARAMETER_ERROR;
          return;
        }

        self[n].state              = E_STATE_RECEIVE_DATA;
        self[n].response_buffer[0] = 0x00;  /* R1. */
        self[n].response_length    = 1;
        return;

      case E_CMD_APP_SEND_OP_COND:
        if (self[n].in_app_cmd) {
          self[n].in_app_cmd         = 0;
          self[n].state              = E_STATE_READY;
          self[n].response_buffer[0] = TOKEN_BUSY;
          self[n].response_length    = 1;
          return;
        }
        break;
 
      case E_CMD_APP_CMD:
        self[n].in_app_cmd = 1;
        return;

      case E_CMD_READ_OCR:
        self[n].state              = E_STATE_SENDING_DATA;
        self[n].response_buffer[0] = 0x00;                               /* R1. */
        self[n].response_buffer[1] = 0x80 | (1 - self[n].is_sdsc) << 6;  /* Powered-up + CCS. */
        self[n].response_buffer[2] = 0;
        self[n].response_buffer[3] = 0;
        self[n].response_buffer[4] = 0;
        self[n].response_length    = 5;
        return;

      default:
        log_wrn("sdcard%d: received unimplemented CMD%d ($%02X $%02X $%02X $%02X $%02X $%02X)\n", n, self[n].command, self[n].command_buffer[0], self[n].command_buffer[1], self[n].command_buffer[2], self[n].command_buffer[3], self[n].command_buffer[4], self[n].command_buffer[5]);
        break;
    }
  }

  log_dbg("sdcard%d: signalling error\n", n);
  self[n].state      = E_STATE_IDLE;
  self[n].error      = E_ERROR_ILLEGAL_COMMAND;
  self[n].in_app_cmd = 0;
}


static void sdcard_block_write(sdcard_nr_t n) {
  log_dbg("sdcard%d: writing %u bytes to %s\n", n, self[n].block_length, SDCARD_IMAGE);

  /* Assume we reject the data. */
  self[n].response_index     = 0;
  self[n].response_length    = 1;
  self[n].response_buffer[0] = 0x0D;  /* Data rejected. */

  if (self[n].data_buffer[0] != TOKEN_START_DATA) {
    log_wrn("sdcard%d: block of data to write did not start with start-of-data token\n", n);
    return;
  }

  if (fwrite(&self[n].data_buffer[1], self[n].block_length, 1, self[n].fp) != 1) {
    log_err("sdcard%d: error writing %u bytes to %s\n", n, self[n].block_length, SDCARD_IMAGE);
    return;
  }

  /* Ensure everything is written. */
  (void) fflush(self[n].fp);
  
  self[n].response_buffer[0] = 0x05;  /* Data accepted. */
  self[n].response_buffer[1] = TOKEN_NOT_BUSY;
  self[n].response_length    = 2;
}


static void sdcard_receive_data(sdcard_nr_t n, u8_t value) {
  /* Preconditions:
   * - self[n].block_length + 2 <= sizeof(self[n].data_buffer)
   * - self[n].data_index       <  sizeof(self[n].data_buffer)
   */
  self[n].data_buffer[self[n].data_index++] = value;

  if (self[n].data_index == 1 + self[n].block_length + 2) {
    sdcard_block_write(n);

    self[n].data_index = 0;
    self[n].state      = E_STATE_TRANSFER;
  }
}


static void sdcard_receive_command(sdcard_nr_t n, u8_t value) {
  if (self[n].command_length == sizeof(self[n].command_buffer)) {
    self[n].command_length = 0;
  }

  if (self[n].command_length == 0) {
    if (value >> 6 != 0x01) {
      log_wrn("sdcard%d: invalid first byte $%02X of command\n", n, value);
      return;
    }
  } else if (self[n].command_length == sizeof(self[n].command_buffer) - 1) {
    if ((value & 0x01) != 0x01) {
      log_wrn("sdcard%d: invalid last byte $%02X of command\n", n, value);
      self[n].command_length = 0;
      return;
    }
  }

  self[n].command_buffer[self[n].command_length] = value;
  self[n].command_length++;

  if (self[n].command_length == sizeof(self[n].command_buffer)) {
    self[n].command = self[n].command_buffer[0] & 0x3F;
    sdcard_handle_command(n);
  }
}


void sdcard_write(sdcard_nr_t n, u16_t address, u8_t value) {
  if (self[n].state == E_STATE_RECEIVE_DATA) {
    sdcard_receive_data(n, value);
  } else {
    sdcard_receive_command(n, value);
  }
}
