#include "ay.h"
#include "clock.h"
#include "divmmc.h"
#include "dma.h"
#include "nextreg.h"
#include "dac.h"
#include "defs.h"
#include "i2c.h"
#include "io.h"
#include "joystick.h"
#include "layer2.h"
#include "log.h"
#include "mf.h"
#include "mouse.h"
#include "paging.h"
#include "spi.h"
#include "sprites.h"
#include "uart.h"
#include "ula.h"


typedef enum {
  E_IO_FUNC_FIRST = 0,
  E_IO_FUNC_TIMEX = E_IO_FUNC_FIRST,
  E_IO_FUNC_PAGING_128K,
  E_IO_FUNC_PAGING_NEXT_BANK,
  E_IO_FUNC_PAGING_PLUS_3,
  E_IO_FUNC_FLOATING_BUS_PLUS_3,
  E_IO_FUNC_DMA_ZXN,
  E_IO_FUNC_KEMPSTON_1,
  E_IO_FUNC_KEMPSTON_2,
  E_IO_FUNC_DIVMMC,
  E_IO_FUNC_MF,
  E_IO_FUNC_I2C,
  E_IO_FUNC_SPI,
  E_IO_FUNC_UART,
  E_IO_FUNC_MOUSE,
  E_IO_FUNC_SPRITES,
  E_IO_FUNC_LAYER_2,
  E_IO_FUNC_AY,
  E_IO_FUNC_DAC_SOUNDRIVE_MODE_1,
  E_IO_FUNC_DAC_SOUNDRIVE_MODE_2,
  E_IO_FUNC_DAC_STEREO_PROFI_COVOX,
  E_IO_FUNC_DAC_STEREO_COVOX,
  E_IO_FUNC_DAC_MONO_PENTAGON_ATM,
  E_IO_FUNC_DAC_MONO_GS_COVOX,
  E_IO_FUNC_DAC_MONO_SPECDRUM,
  E_IO_FUNC_ULAPLUS,
  E_IO_FUNC_DMA_Z80,
  E_IO_FUNC_PENTAGON_1024_MEMORY,
  E_IO_FUNC_Z80_CTC,
  E_IO_FUNC_LAST = E_IO_FUNC_Z80_CTC
} io_func_t;


typedef struct {
  int  is_enabled[E_IO_FUNC_LAST - E_IO_FUNC_FIRST + 1];
  u8_t mf_port_enable;
  u8_t mf_port_disable;
} io_t;


static io_t self;


int io_init(void) {
  io_reset(E_RESET_HARD);
  return 0;
}


void io_reset(reset_t reset) {
  /* Enable all ports. */
  memset(self.is_enabled, 0xFF, sizeof(self.is_enabled));

  self.mf_port_enable  = 0x3F;
  self.mf_port_disable = 0xBF;
}


void io_finit(void) {
}


void io_mf_ports_set(u8_t enable, u8_t disable) {
  self.mf_port_enable  = enable;
  self.mf_port_disable = disable;
}


/**
 * https://worldofspectrum.org/faq/reference/48kreference.htm#IOContention
 */
static void io_contend(u16_t address) {
  const u8_t high_byte = address >> 8;
  const u8_t low_bit   = address & 1;

  if (high_byte < 0x40 || high_byte > 0x7F) {
    if (low_bit) {
      /* N:4 */
      clock_run(4);
    } else {
      /* N:1, C:3 */
      clock_run(1);
      ula_contend();
      clock_run(3);
    }
  } else if (low_bit) {
    /* C:1, C:1, C:1, C:1 */
    ula_contend();
    clock_run(1);
    ula_contend();
    clock_run(1);
    ula_contend();
    clock_run(1);
    ula_contend();
    clock_run(1);
  } else {
    /* C:1, C:3 */
    ula_contend();
    clock_run(1);
    ula_contend();
    clock_run(3);
  }
}


u8_t io_read(u16_t address) {
  io_contend(address);

  if ((address & 0x0001) == 0x0000) {
    return ula_read(address);
  }

  switch (address) {
    case 0x113B:
      return self.is_enabled[E_IO_FUNC_I2C]
        ? i2c_sda_read(address)
        : ula_floating_bus_read();

    case 0x123B:
      return self.is_enabled[E_IO_FUNC_LAYER_2]
        ? layer2_access_read()
        : ula_floating_bus_read();

    case 0x133B:
      return self.is_enabled[E_IO_FUNC_UART]
        ? uart_tx_read()
        : ula_floating_bus_read();

    case 0x143B:
      return self.is_enabled[E_IO_FUNC_UART]
        ? uart_rx_read()
        : ula_floating_bus_read();

    case 0x153B:
      return self.is_enabled[E_IO_FUNC_UART]
        ? uart_select_read()
        : ula_floating_bus_read();

    case 0x163B:
      return self.is_enabled[E_IO_FUNC_UART]
        ? uart_frame_read()
        : ula_floating_bus_read();

    case 0x1FFD:
      return self.is_enabled[E_IO_FUNC_PAGING_PLUS_3]
        ? paging_spectrum_plus_3_paging_read()
        : ula_floating_bus_read();

    case 0x243B:
      return nextreg_select_read(address);

    case 0x253B:
      return nextreg_data_read(address);

    case 0x7FFD:
      return self.is_enabled[E_IO_FUNC_PAGING_128K]
        ? paging_spectrum_128k_paging_read()
        : ula_floating_bus_read();

    case 0xDFFD:
      return self.is_enabled[E_IO_FUNC_PAGING_NEXT_BANK]
        ? paging_spectrum_next_bank_extension_read()
        : ula_floating_bus_read();

    case 0xFBDF:
      return self.is_enabled[E_IO_FUNC_MOUSE]
        ? mouse_read_x()
        : ula_floating_bus_read();

    case 0xFFDF:
      return self.is_enabled[E_IO_FUNC_MOUSE]
        ? mouse_read_y()
        : ula_floating_bus_read();

    case 0xFADF:
      return self.is_enabled[E_IO_FUNC_MOUSE]
        ? mouse_read_buttons()
        : ula_floating_bus_read();

    default:
      break;
  } 

  if ((address & 0xC007) == 0xC005) {
    return self.is_enabled[E_IO_FUNC_AY]
      ? ay_register_read()
      : ula_floating_bus_read();
  }

  if ((address & 0x00FF) == self.mf_port_enable) {
    return self.is_enabled[E_IO_FUNC_MF]
      ? mf_enable_read(address)
      : ula_floating_bus_read();
  }

  if ((address & 0x00FF) == self.mf_port_disable) {
    return self.is_enabled[E_IO_FUNC_MF]
      ? mf_disable_read(address)
      : ula_floating_bus_read();
  }

  switch (address & 0x00FF) {
    case 0x0B:
      return self.is_enabled[E_IO_FUNC_DMA_Z80]
        ? dma_read(address)
        : ula_floating_bus_read();

    case 0x1F:
      return self.is_enabled[E_IO_FUNC_KEMPSTON_1]
        ? joystick_kempston_read(E_JOYSTICK_LEFT)
        : ula_floating_bus_read();

    case 0x37:
      return self.is_enabled[E_IO_FUNC_KEMPSTON_2]
        ? joystick_kempston_read(E_JOYSTICK_RIGHT)
        : ula_floating_bus_read();

    case 0x6B:
      return self.is_enabled[E_IO_FUNC_DMA_ZXN]
        ? dma_read(address)
        : ula_floating_bus_read();

    case 0xE3:
      return self.is_enabled[E_IO_FUNC_DIVMMC]
        ? divmmc_control_read(address)
        : ula_floating_bus_read();

    case 0xE7:
      return self.is_enabled[E_IO_FUNC_SPI]
        ? spi_cs_read(address)
        : ula_floating_bus_read();

    case 0xEB:
      return self.is_enabled[E_IO_FUNC_SPI]
        ? spi_data_read(address)
        : ula_floating_bus_read();

    case 0xFF:
      return self.is_enabled[E_IO_FUNC_TIMEX]
        ? ula_timex_read(address)
        : ula_floating_bus_read();

    default:
      break;
  }

  log_wrn("io: unimplemented read from $%04X\n", address);

  return ula_floating_bus_read();
}


void io_write(u16_t address, u8_t value) {
  io_contend(address);

  if ((address & 0x0001) == 0x0000) {
    ula_write(address, value);
    return;
  }

  switch (address) {
    case 0x103B:
      if (self.is_enabled[E_IO_FUNC_I2C]) {
        i2c_scl_write(address, value);
      }
      return;

    case 0x113B:
      if (self.is_enabled[E_IO_FUNC_I2C]) {
        i2c_sda_write(address, value);
      }
      return;

    case 0x123B:
      if (self.is_enabled[E_IO_FUNC_LAYER_2]) {
        layer2_access_write(value);
      }
      return;

    case 0x133B:
      if (self.is_enabled[E_IO_FUNC_UART]) {
        uart_tx_write(value);
      }
      return;

    case 0x143B:
      if (self.is_enabled[E_IO_FUNC_UART]) {
        uart_rx_write(value);
      }
      return;

    case 0x153B:
      if (self.is_enabled[E_IO_FUNC_UART]) {
        uart_select_write(value);
      }
      return;

    case 0x163B:
      if (self.is_enabled[E_IO_FUNC_UART]) {
        uart_frame_write(value);
      }
      return;

    case 0x1FFD:
      if (self.is_enabled[E_IO_FUNC_PAGING_PLUS_3]) {
        paging_spectrum_plus_3_paging_write(value);
      }
      return;

    case 0x243B:
      nextreg_select_write(address, value);
      return;

    case 0x253B:
      nextreg_data_write(address, value);
      return;

    case 0x303B:
      if (self.is_enabled[E_IO_FUNC_SPRITES]) {
        sprites_slot_set(value);
      }
      return;

    case 0x7FFD:
      if (self.is_enabled[E_IO_FUNC_PAGING_128K]) {
        paging_spectrum_128k_paging_write(value);
      }
      return;

    case 0xDFFD:
      if (self.is_enabled[E_IO_FUNC_PAGING_NEXT_BANK]) {
        paging_spectrum_next_bank_extension_write(value);
      }
      return;

    default:
      break;
  }

  if (self.is_enabled[E_IO_FUNC_MF]) {
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
      if (self.is_enabled[E_IO_FUNC_AY]) {
        ay_register_select(value);
      }
      return;

    case 0x8005:  /* 0xBFFD */
      if (self.is_enabled[E_IO_FUNC_AY]) {
        ay_register_write(value);
      }
      return;

    default:
      break;
  }

  switch (address & 0x00FF) {
    case 0x0B:
      if (self.is_enabled[E_IO_FUNC_DMA_Z80]) {
        dma_write(address, value);
      }
      break;

    case 0x6B:
      if (self.is_enabled[E_IO_FUNC_DMA_ZXN]) {
        dma_write(address, value);
      }
      return;

    case 0xE3:
      if (self.is_enabled[E_IO_FUNC_DIVMMC]) {
        divmmc_control_write(address, value);
      }
      return;

    case 0xE7:
      if (self.is_enabled[E_IO_FUNC_SPI]) {
        spi_cs_write(address, value);
      }
      return;

    case 0xEB:
      if (self.is_enabled[E_IO_FUNC_SPI]) {
        spi_data_write(address, value);
      }
      return;

    case 0xFF:
      if (self.is_enabled[E_IO_FUNC_TIMEX]) {
        ula_timex_write(address, value);
      }
      return;

    case 0x0F:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_1] ||
          self.is_enabled[E_IO_FUNC_DAC_STEREO_COVOX]) {
        dac_write(DAC_B, value);
      }
      return;

    case 0x1F:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_1]) {
        dac_write(DAC_A, value);
      }
      return;

    case 0x3F:
      if (self.is_enabled[E_IO_FUNC_DAC_STEREO_PROFI_COVOX]) {
        dac_write(DAC_A, value);
      }
      return;

    case 0x4F:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_1] ||
          self.is_enabled[E_IO_FUNC_DAC_STEREO_COVOX]) {
        dac_write(DAC_C, value);
      }
      return;

    case 0x5F:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_1] ||
          self.is_enabled[E_IO_FUNC_DAC_STEREO_PROFI_COVOX]) {
        dac_write(DAC_D, value);
      }
      return;

    case 0xB3:
      if (self.is_enabled[E_IO_FUNC_DAC_MONO_GS_COVOX]) {
        dac_write(DAC_B | DAC_C, value);
      }
      return;

    case 0xDF:
      if (self.is_enabled[E_IO_FUNC_DAC_MONO_SPECDRUM]) {
        dac_write(DAC_A | DAC_D, value);
      }
      return;

    case 0xF1:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_2]) {
        dac_write(DAC_A, value);
      }
      return;

    case 0xF3:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_2]) {
        dac_write(DAC_B, value);
      }
      return;

    case 0xF9:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_2]) {
        dac_write(DAC_C, value);
      }
      return;

    case 0xFB:
      if (self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_2] ||
          self.is_enabled[E_IO_FUNC_DAC_MONO_PENTAGON_ATM]) {
        dac_write(DAC_A | DAC_D, value);
      }
      return;

    case 0x57:
      if (self.is_enabled[E_IO_FUNC_SPRITES]) {
        sprites_next_attribute_set(value);
      }
      return;

    case 0x5B:
      if (self.is_enabled[E_IO_FUNC_SPRITES]) {
        sprites_next_pattern_set(value);
      }
      return;

    default:
      break;
  }

  log_wrn("io: unimplemented write of $%02X to $%04X\n", value, address);
}


void io_decoding_write(u8_t index, u8_t value) {
  /* TODO: Reset when bits 0:30 are 1 (bit 31=0: soft, 1: hard). */
  switch (index) {
    case 0:
      self.is_enabled[E_IO_FUNC_TIMEX]               = value & 0x01;
      self.is_enabled[E_IO_FUNC_PAGING_128K]         = value & 0x02;
      self.is_enabled[E_IO_FUNC_PAGING_NEXT_BANK]    = value & 0x04;
      self.is_enabled[E_IO_FUNC_PAGING_PLUS_3]       = value & 0x08;
      self.is_enabled[E_IO_FUNC_FLOATING_BUS_PLUS_3] = value & 0x10;
      self.is_enabled[E_IO_FUNC_DMA_ZXN]             = value & 0x20;
      self.is_enabled[E_IO_FUNC_KEMPSTON_1]          = value & 0x40;
      self.is_enabled[E_IO_FUNC_KEMPSTON_2]          = value & 0x80;
      break;

    case 1:
      self.is_enabled[E_IO_FUNC_DIVMMC]  = value & 0x01;
      self.is_enabled[E_IO_FUNC_MF]      = value & 0x02;
      self.is_enabled[E_IO_FUNC_I2C]     = value & 0x04;
      self.is_enabled[E_IO_FUNC_SPI]     = value & 0x08;
      self.is_enabled[E_IO_FUNC_UART]    = value & 0x10;
      self.is_enabled[E_IO_FUNC_MOUSE]   = value & 0x20;
      self.is_enabled[E_IO_FUNC_SPRITES] = value & 0x40;
      self.is_enabled[E_IO_FUNC_LAYER_2] = value & 0x80;
      break;

    case 2:
      self.is_enabled[E_IO_FUNC_AY]                     = value & 0x01;
      self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_1]   = value & 0x02;
      self.is_enabled[E_IO_FUNC_DAC_SOUNDRIVE_MODE_2]   = value & 0x04;
      self.is_enabled[E_IO_FUNC_DAC_STEREO_PROFI_COVOX] = value & 0x08;
      self.is_enabled[E_IO_FUNC_DAC_STEREO_COVOX]       = value & 0x10;
      self.is_enabled[E_IO_FUNC_DAC_MONO_PENTAGON_ATM]  = value & 0x20;
      self.is_enabled[E_IO_FUNC_DAC_MONO_GS_COVOX]      = value & 0x40;
      self.is_enabled[E_IO_FUNC_DAC_MONO_SPECDRUM]      = value & 0x80;
      break;

    case 3:
      self.is_enabled[E_IO_FUNC_ULAPLUS]              = value & 0x01;
      self.is_enabled[E_IO_FUNC_DMA_Z80]              = value & 0x02;
      self.is_enabled[E_IO_FUNC_PENTAGON_1024_MEMORY] = value & 0x04;
      self.is_enabled[E_IO_FUNC_Z80_CTC]              = value & 0x08;
      break;

    default:
      break;
  }
}
