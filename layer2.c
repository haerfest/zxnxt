#include <string.h>
#include "defs.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "palette.h"


typedef enum {
  E_RESOLUTION_256X192 = 0,
  E_RESOLUTION_320x256,
  E_RESOLUTION_640X256
} resolution_t;


typedef enum {
  E_MAPPING_FIRST_16K = 0,
  E_MAPPING_SECOND_16K,
  E_MAPPING_THIRD_16K,
  E_MAPPING_FIRST_48K
} mapping_t;


typedef struct {
  u8_t*        ram;
  u8_t         access;
  u8_t         control;
  u8_t         active_bank;
  u8_t         shadow_bank;
  int          is_visible;
  int          is_readable;
  int          is_writable;
  int          do_map_shadow;
  mapping_t    mapping;
  resolution_t resolution;
  u8_t         palette_offset;
  u8_t         bank_offset;
} self_t;


static self_t self;


int layer2_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));

  self.ram = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM];

  layer2_reset();

  return 0;
}


void layer2_finit(void) {
}


void layer2_reset(void) {
  layer2_access_write(0x00);
  layer2_access_write(0x10);
  layer2_active_bank_write(8);
  layer2_shadow_bank_write(11);
}


void layer2_access_write(u8_t value) {
  log_dbg("layer2: access write $%02X\n", value);

  if (value & 0x10)
  {
    self.bank_offset = value & 0x07;
  }
  else
  {
    self.mapping       = value >> 6;
    self.do_map_shadow = value & 0x08;
    self.is_readable   = value & 0x04;
    self.is_visible    = value & 0x02;
    self.is_writable   = value & 0x01;

    if (self.mapping == E_MAPPING_FIRST_48K) {
      log_dbg("layer2: first 48K mapping not yet implemented\n");
    }

    memory_refresh_accessors(0, 2);
  }
}


void layer2_control_write(u8_t value) {
  log_dbg("layer2: control write $%02X\n", value);
  self.resolution     = (value & 0x30) >> 4;
  self.palette_offset = value & 0x0F;
}


void layer2_active_bank_write(u8_t bank) {
  log_dbg("layer2: active bank write $%02X\n", bank);
  if (bank != self.active_bank) {
    self.active_bank = bank;
  }
}


void layer2_shadow_bank_write(u8_t bank) {
  log_dbg("layer2: shadow bank write $%02X\n", bank);
  if (bank != self.shadow_bank) {
    self.shadow_bank = bank;
  }
}


void layer2_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  palette_entry_t colour;
  u8_t            palette_index;

  *is_transparent = 1;

  if (!self.is_visible) {
    return;
  }

  switch (self.resolution) {
    case E_RESOLUTION_256X192:
      if (row < 32 || row >= 32 + 192 || column < 32 * 2 || column >= (32 + 256) * 2) {
        /* Border. */
        return;
      }
      palette_index   = self.ram[self.active_bank * 16 * 1024 + (row - 32) * 256 + (column - 32 * 2) / 2];
      colour          = palette_read_rgb(E_PALETTE_LAYER2_FIRST, (self.palette_offset << 4) + palette_index);
      *rgba           = colour.red << 12 | colour.green << 8 | colour.blue << 4;
      *is_transparent = 0;
      break;

    default:
      log_dbg("layer2: resolution #%d not implemented yet\n", self.resolution);
      break;
  }
}


int layer2_is_readable(void) {
  return self.is_readable;
}


int layer2_is_writable(void) {
  return self.is_writable;
}


static u32_t layer2_translate(u16_t address) {
  const u8_t bank = (self.do_map_shadow ? self.shadow_bank : self.active_bank) + self.bank_offset;

  switch (self.mapping) {
    case E_MAPPING_FIRST_16K:
    case E_MAPPING_FIRST_48K:
      return bank * 16 * 1024 + address;

    case E_MAPPING_SECOND_16K:
      return (bank + 1) * 16 * 1024 + address;

    case E_MAPPING_THIRD_16K:
      return (bank + 2) * 16 * 1024 + address;
  }
}


u8_t layer2_read(u16_t address) {
  return self.ram[layer2_translate(address)];
}


void layer2_write(u16_t address, u8_t value) {
  self.ram[layer2_translate(address)] = value;
}
