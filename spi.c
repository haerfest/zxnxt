#include <stdio.h>
#include "defs.h"
#include "sdcard.h"


typedef enum {
  E_SPI_DEVICE_NONE = 0,
  E_SPI_DEVICE_SDCARD_0,
  E_SPI_DEVICE_SDCARD_1,
  E_SPI_DEVICE_PI_0,
  E_SPI_DEVICE_PI_1,
  E_SPI_DEVICE_FPGA_FLASH
} spi_device_t;


static const char* spi_device_names[] = {
  "no device",
  "first SD card",
  "second SD card",
  "first Raspberry Pi",
  "second Raspberry Pi",
  "FPGA flash"
};


typedef struct {
  spi_device_t device;
} spi_t;


static spi_t self;


int spi_init(void) {
  self.device = E_SPI_DEVICE_NONE;
  return 0;
}


void spi_finit(void) {
}


u8_t spi_cs_read(u16_t address) {
  switch (self.device) {
    case E_SPI_DEVICE_SDCARD_0:
      return (u8_t) ~0x01;

    case E_SPI_DEVICE_SDCARD_1:
      return (u8_t) ~0x02;

    case E_SPI_DEVICE_PI_0:
      return (u8_t) ~0x04;

    case E_SPI_DEVICE_PI_1:
      return (u8_t) ~0x08;

    case E_SPI_DEVICE_FPGA_FLASH:
      return (u8_t) ~0x80;

    default:
      return (u8_t) ~0x00;
  }
}


void spi_cs_write(u16_t address, u8_t value) {
  int n_devices_selected = 0;

  if ((value & 0x01) == 0) {
    self.device = E_SPI_DEVICE_SDCARD_0;
    n_devices_selected++;
  }
  if ((value & 0x02) == 0) {
    self.device = E_SPI_DEVICE_SDCARD_1;
    n_devices_selected++;
  } 
  if ((value & 0x04) == 0) {
    self.device = E_SPI_DEVICE_PI_0;
    n_devices_selected++;
  }
  if ((value & 0x08) == 0) {
    self.device = E_SPI_DEVICE_PI_1;
    n_devices_selected++;
  }
  if ((value & 0x80) == 0) {
    self.device = E_SPI_DEVICE_FPGA_FLASH;
    n_devices_selected++;
  }

  if (n_devices_selected != 1) {
    self.device = E_SPI_DEVICE_NONE;
  }

  fprintf(stderr, "spi: %s selected\n", spi_device_names[self.device]);
}


u8_t spi_data_read(u16_t address) {
  switch (self.device) {
    case E_SPI_DEVICE_SDCARD_0:
      return sdcard_read(E_SDCARD_0, address);

    case E_SPI_DEVICE_SDCARD_1:
      return sdcard_read(E_SDCARD_1, address);

    case E_SPI_DEVICE_NONE:
      return 0xFF;

    default:
      fprintf(stderr, "spi: unimplemented read from %s\n", spi_device_names[self.device]);
      return 0xFF;
  }
}


void spi_data_write(u16_t address, u8_t value) {
  switch (self.device) {
    case E_SPI_DEVICE_SDCARD_0:
      sdcard_write(E_SDCARD_0, address, value);
      break;

    case E_SPI_DEVICE_SDCARD_1:
      sdcard_write(E_SDCARD_1, address, value);
      break;

    case E_SPI_DEVICE_NONE:
      break;

    default:
      fprintf(stderr, "spi: unimplemented write of $%02X to %s\n", value, spi_device_names[self.device]);
      break;
  }
}
