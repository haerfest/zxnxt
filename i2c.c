#include <string.h>
#include "defs.h"
#include "i2c.h"
#include "log.h"
#include "rtc.h"


typedef u8_t (*reader_t)(void);
typedef void (*writer_t)(u8_t);


const struct slave_t {
  u8_t     address;
  reader_t read;
  writer_t write;
} slaves[] = {
  { 0x68, rtc_read, rtc_write }
};


typedef enum state_t {
  E_STATE_IDLE = 0,
  E_STATE_ADDRESS,
  E_STATE_DIRECTION,
  E_STATE_DIRECTION_ACK,
  E_STATE_DATA,
  E_STATE_DATA_ACK
} state_t;


typedef struct i2c_t {
  state_t  state;
  u8_t     scl;
  u8_t     sda;
  int      bit_count;
  int      does_master_write;
  u8_t     address;
  u8_t     data;
  reader_t slave_read;
  writer_t slave_write;
} i2c_t;


static i2c_t self;


int i2c_init(void) {
  memset(&self, 0, sizeof(self));
  i2c_reset(E_RESET_HARD);
  return 0;
}


void i2c_finit(void) {
}


void i2c_reset(reset_t reset) {
  self.state             = E_STATE_IDLE;
  self.slave_read        = NULL;
  self.slave_write       = NULL;
  self.bit_count         = 0;
  self.does_master_write = 0;
  self.address           = 0;
  self.data              = 0;

  /* Not busy. */
  self.sda = 1;
  self.scl = 1;
}


static int is_slave_prepared(void) {
  size_t i;

  self.slave_read  = NULL;
  self.slave_write = NULL;

  for (i = 0; i < sizeof(slaves) / sizeof(*slaves); i++) {
    if (slaves[i].address == self.address) {
      self.slave_read  = slaves[i].read;
      self.slave_write = slaves[i].write;
      return 1;
    }
  }

  return 0;
}


void i2c_scl_write(u16_t address, u8_t value) {
  const int did_clock_rise = !self.scl && (value & 0x01);

  self.scl = value & 0x01;

  if (!did_clock_rise) {
    return;
  }

  switch (self.state) {
    case E_STATE_ADDRESS:
      /* Master is sending 7-bit slave address. */
      self.address = (self.address << 1) | self.sda;
      if (++self.bit_count == 7) {
        self.bit_count = 0;
        self.state     = E_STATE_DIRECTION;
      }
      break;

    case E_STATE_DIRECTION:
      /* Master is sending direction of transfer. */
      self.does_master_write = !self.sda;
      self.state             = is_slave_prepared() ? E_STATE_DIRECTION_ACK : E_STATE_IDLE;
      break;

    case E_STATE_DIRECTION_ACK:
      /* Slave must acknowledge having received from Master by holding SDA low. */
      self.sda       = 0x00;
      self.state     = E_STATE_DATA;
      self.bit_count = 0;
      if (!self.does_master_write) {
        self.data    = self.slave_read();
      }
      break;

    case E_STATE_DATA:
      if (self.does_master_write) {
        /* Master is sending 8-bit data. */
        self.data = (self.data << 1) | self.sda;
        if (++self.bit_count == 8) {
          self.slave_write(self.data);
          self.state = E_STATE_DATA_ACK;
        }
      } else {
        /* Master reads 8-bit data from slave. */
        self.sda    = (self.data & 0x80) ? 0xFF : 0x00;
        self.data <<= 1;
        if (++self.bit_count == 8) {
          /* Master must acknowledge having read data from slave. */
          self.state = E_STATE_DATA_ACK;
        }
      }
      break;

    case E_STATE_DATA_ACK:
      if (self.does_master_write) {
        /* Slave must acknowledge having received from Master by holding SDA low. */
        self.sda       = 0x00;
        self.state     = E_STATE_DATA;
        self.bit_count = 0;
      } else {
        /* Master must acknowledge having received from slave by holding SLA low. */
        if (self.sda) {
          self.state = E_STATE_IDLE;
        } else {
          self.state     = E_STATE_DATA;
          self.data      = self.slave_read();
          self.bit_count = 0;
        }
      }
      break;

    default:
      self.sda = 0xFF;
      break;
  }
}


void i2c_sda_write(u16_t address, u8_t value) {
  const u8_t sda = value & 0x01;

  if (self.scl) {
    if (self.sda && !sda) {
      /* A high to low SDA with SCL high indicates start. */
      self.state     = E_STATE_ADDRESS;
      self.bit_count = 0;
    } else if (!self.sda && sda) {
      /* A low to high SDA with SCL high indicates stop. */
      self.state = E_STATE_IDLE;
    }
  }

  self.sda = sda;
}


u8_t i2c_sda_read(u16_t address) {
  return self.sda;
}
