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
} sdcard_t;


static u16_t crc16_table[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5,
  0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b,
  0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
  0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c,
  0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
  0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b,
  0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
  0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
  0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
  0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969,
  0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
  0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
  0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03,
  0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
  0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6,
  0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
  0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
  0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1,
  0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c,
  0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2,
  0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
  0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447,
  0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
  0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2,
  0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
  0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
  0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
  0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0,
  0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d,
  0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
  0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba,
  0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
  0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};


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
  fprintf(stderr, "sdcard: read\n");

  if (self[n].response_index < self[n].response_length) {
    return self[n].response_buffer[self[n].response_index++];
  }

  /* Standard R1 response. */
  return self[n].error | (self[n].state == E_STATE_IDLE);
}


/* See http://automationwiki.com/index.php?title=CRC-16-CCITT. */
static u16_t crc16(const u8_t* data, u32_t length) { 
   u32_t i;
   u16_t crc;
   u16_t index;

   crc = 0xFFFF;
   for (i = 0; i < length; i++) {
     index = (data[i] ^ (crc >> 8)) & 0xff;
     crc   = crc16_table[index] ^ (crc << 8);
   }

   return crc;
}


static void sdcard_handle_command(sdcard_nr_t n) {
  const u8_t command = self[n].command_buffer[0] & 0x3F;
  u32_t      block_length;
  u16_t      crc;
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
      crc = crc16(&self[n].response_buffer[1], 16);
      self[n].response_buffer[17] = crc >> 8;
      self[n].response_buffer[18] = crc & 0x00FF;
      self[n].response_length     = 19;
      return;
      
    case E_CMD_STOP_TRANSMISSION:
      self[n].state              = E_STATE_TRANSFER;
      self[n].response_buffer[0] = 0x01;
      self[n].response_buffer[1] = 0xFF;  /* No longer busy. */
      self[n].response_length    = 2;
      return;

    case E_CMD_SET_BLOCKLEN:
      block_length = self[n].command_buffer[1] << 24 | self[n].command_buffer[2] << 16 | self[n].command_buffer[3] << 8 | self[n].command_buffer[4];
      if (block_length > MAX_RESPONSE_PAYLOAD_SIZE) {
        fprintf(stderr, "sdcard: unsupported block length %u bytes\n", block_length);
        break;
      }
      self[n].block_length = block_length;
      return;

    case E_CMD_READ_SINGLE_BLOCK:
      /* Assume failure. */
      self[n].response_buffer[0] = 0x01;
      self[n].response_length    = 1;

      if (n == E_SDCARD_0) {
        FILE* fp;
        long  offset;

        fp = fopen(SDCARD_IMAGE, "rb");
        if (fp == NULL) {
          fprintf(stderr, "sdcard: error opening %s for reading\n", SDCARD_IMAGE);
          return;
        }

        offset = self[n].command_buffer[1] << 24 | self[n].command_buffer[2] << 16 | self[n].command_buffer[3] << 8 | self[n].command_buffer[4];
        if (fseek(fp, offset, SEEK_SET) != 0) {
          fprintf(stderr, "sdcard: error seeking to position %ld in %s\n", offset, SDCARD_IMAGE);
          fclose(fp);
          return;
        }

        if (fread(&self[n].response_buffer[2], self[n].block_length, 1, fp) != 1) {
          fprintf(stderr, "sdcard: error reading %u bytes from offset %ld in %s\n", self[n].block_length, offset, SDCARD_IMAGE);
          fclose(fp);
          return;
        }

        fclose(fp);

        crc = crc16(&self[n].response_buffer[2], self[n].block_length);
        self[n].response_buffer[0]                            = 0x00;  /* R1. */
        self[n].response_buffer[1]                            = 0xFE;  /* Start block token. */
        self[n].response_buffer[2 + self[n].block_length + 0] = crc >> 7;
        self[n].response_buffer[2 + self[n].block_length + 1] = crc & 0x00FF;
        self[n].response_length                               = 2 + self[n].block_length + 2;
      }
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
