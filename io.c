#include "divmmc.h"
#include "nextreg.h"
#include "dac.h"
#include "defs.h"
#include "layer2.h"
#include "log.h"
#include "i2c.h"
#include "paging.h"
#include "spi.h"
#include "timex.h"
#include "ula.h"


int io_init(void) {
  return 0;
}


void io_finit(void) {
}


u8_t io_read(u16_t address) {
  if ((address & 0x0001) == 0x0000) {
    return ula_read(address);
  }

  switch (address & 0x00FF) {
    case 0x00E3:
      return divmmc_control_read(address);

    case 0xE7:
      return spi_cs_read(address);

    case 0xEB:
      return spi_data_read(address);

    case 0xFF:
      return timex_read(address);

    case 0x0F:
    case 0x1F:
    case 0x3F:
    case 0x4F:
    case 0x5F:
    case 0xB3:
    case 0xDF:
    case 0xF1:
    case 0xF3:
    case 0xF9:
    case 0xFB:
      return dac_read(address);

    default:
      break;
  }

  switch (address) {
    case 0x113B:
      return i2c_sda_read(address);

    case 0x123B:
      return layer2_read(address);

    case 0x1FFD:
      return paging_spectrum_plus_3_paging_read();

    case 0x243B:
      return nextreg_select_read(address);

    case 0x253B:
      return nextreg_data_read(address);

    case 0x7FFD:
      return paging_spectrum_128k_paging_read();

    case 0xDFFD:
      paging_spectrum_plus_3_paging_read();
      break;

    default:
      break;
  } 

  log_wrn("io: unimplemented read from $%04X\n", address);

  return 0xFF;
}


void io_write(u16_t address, u8_t value) {
  if ((address & 0x0001) == 0x0000) {
    ula_write(address, value);
    return;
  }

  switch (address & 0x00FF) {
    case 0x00E3:
      divmmc_control_write(address, value);
      return;

    case 0xE7:
      spi_cs_write(address, value);
      return;

    case 0xEB:
      spi_data_write(address, value);
      return;

    case 0xFF:
      timex_write(address, value);
      return;

    case 0x0F:
    case 0x1F:
    case 0x3F:
    case 0x4F:
    case 0x5F:
    case 0xB3:
    case 0xDF:
    case 0xF1:
    case 0xF3:
    case 0xF9:
    case 0xFB:
      dac_write(address, value);
      return;

    default:
      break;
  }

  switch (address) {
    case 0x103B:
      i2c_scl_write(address, value);
      return;

    case 0x113B:
      i2c_sda_write(address, value);
      return;

    case 0x123B:
      layer2_write(address, value);
      return;

    case 0x1FFD:
      paging_spectrum_plus_3_paging_write(value);
      break;

    case 0x243B:
      nextreg_select_write(address, value);
      return;

    case 0x253B:
      nextreg_data_write(address, value);
      return;

    case 0x7FFD:
      paging_spectrum_128k_paging_write(value);
      break;

    case 0xDFFD:
      paging_spectrum_next_bank_extension_write(value);
      break;

    default:
      break;
  } 

  log_wrn("io: unimplemented write of $%02X to $%04X\n", value, address);
}
