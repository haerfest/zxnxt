#include "ay.h"
#include "divmmc.h"
#include "dma.h"
#include "nextreg.h"
#include "dac.h"
#include "defs.h"
#include "i2c.h"
#include "io.h"
#include "kempston.h"
#include "layer2.h"
#include "log.h"
#include "mf.h"
#include "paging.h"
#include "spi.h"
#include "ula.h"


typedef struct {
  int  is_port_7FFD_enabled;
  int  is_port_BFFD_enabled;
  int  is_port_FFFD_enabled;
  u8_t mf_port_enable;
  u8_t mf_port_disable;
  int  is_mf_port_decoding_enabled;
} self_t;


static self_t self;


int io_init(void) {
  io_reset();
  return 0;
}


void io_reset(void) {
  self.is_port_7FFD_enabled        = 1;
  self.is_port_BFFD_enabled        = 1;
  self.is_port_FFFD_enabled        = 1;
  self.mf_port_enable              = 0x3F;
  self.mf_port_disable             = 0xBF;
  self.is_mf_port_decoding_enabled = 1;
}


void io_finit(void) {
}


void io_mf_port_decoding_enable(int do_enable) {
  self.is_mf_port_decoding_enabled = do_enable;
}


void io_mf_ports_set(u8_t enable, u8_t disable) {
  self.mf_port_enable  = enable;
  self.mf_port_disable = disable;
}


u8_t io_read(u16_t address) {
  if ((address & 0x0001) == 0x0000) {
    return ula_read(address);
  }

  /* TODO: Bit 14 of address must be set on Plus 3? */
  if ((address & 0x8003) == 0x0001) {
    /* Typically 0x7FFD. */
    return paging_spectrum_128k_paging_read();
  }

  /* TODO: 0xBFFD is readable on +3. */
  if ((address & 0xC007) == 0xC005) {
    /* 0xFFFD */
    return self.is_port_FFFD_enabled ? ay_register_read() : 0xFF;
  }

  if (self.is_mf_port_decoding_enabled) {
    if ((address & 0x00FF) == self.mf_port_enable) {
      return mf_enable_read(address);
    }

    if ((address & 0x00FF) == self.mf_port_disable) {
      return mf_disable_read(address);
    }
  }

  switch (address & 0x00FF) {
    case 0x0B:
    case 0x6B:
      return dma_read(address);

    case 0x1F:
      return kempston_read(address);

    case 0xE3:
      return divmmc_control_read(address);

    case 0xE7:
      return spi_cs_read(address);

    case 0xEB:
      return spi_data_read(address);

    case 0xFF:
      return ula_timex_read(address);

    case 0x0F:
      /* case 0x1F: */
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
      return layer2_access_read();

    case 0x1FFD:
      return paging_spectrum_plus_3_paging_read();

    case 0x243B:
      return nextreg_select_read(address);

    case 0x253B:
      return nextreg_data_read(address);

    case 0x7FFD:
      return paging_spectrum_128k_paging_read();

    case 0xDFFD:
      paging_spectrum_next_bank_extension_read();
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

  if (self.is_mf_port_decoding_enabled) {
    if ((address & 0x00FF) == self.mf_port_enable) {
      mf_enable_write(address, value);
      return;
    }

    if (address == self.mf_port_disable) {
      mf_disable_write(address, value);
      return;
    }
  }

  switch (address & 0xC007) {
    case 0xC005:  /* 0xFFFD */
      if (self.is_port_FFFD_enabled) {
        ay_register_select(value);
      }
      return;

    case 0x8005:  /* 0xBFFD */
      if (self.is_port_BFFD_enabled) {
        ay_register_write(value);
      }
      return;

    default:
      break;
  }

  switch (address & 0x00FF) {
    case 0x0B:
    case 0x6B:
      dma_write(address, value);
      return;

    case 0xE3:
      divmmc_control_write(address, value);
      return;

    case 0xE7:
      spi_cs_write(address, value);
      return;

    case 0xEB:
      spi_data_write(address, value);
      return;

    case 0xFF:
      ula_timex_write(address, value);
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
      layer2_access_write(value);
      return;

    case 0x1FFD:
      paging_spectrum_plus_3_paging_write(value);
      return;

    case 0x243B:
      nextreg_select_write(address, value);
      return;

    case 0x253B:
      nextreg_data_write(address, value);
      return;

    case 0x7FFD:
      if (self.is_port_7FFD_enabled) {
        paging_spectrum_128k_paging_write(value);
      }
      return;

    case 0xDFFD:
      paging_spectrum_next_bank_extension_write(value);
      return;

    default:
      break;
  } 

  log_wrn("io: unimplemented write of $%02X to $%04X\n", value, address);
}


void io_port_enable(u16_t address, int enable) {
  switch (address) {
    case 0x7FFD:
      self.is_port_7FFD_enabled = enable;
      break;

    case 0xBFFD:
      self.is_port_BFFD_enabled = enable;
      break;

    case 0xFFFD:
      self.is_port_FFFD_enabled = enable;
      break;
      
    default:
      log_wrn("io: port $%04X en/disabling not implemented\n", address);
      break;
  }
}
    
