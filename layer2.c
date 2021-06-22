#include <string.h>
#include "defs.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "palette.h"


typedef enum {
  E_RESOLUTION_256X192 = 0,
  E_RESOLUTION_320X256,
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
  u8_t         bank_offset;
  resolution_t resolution;
  palette_t    palette;
  u8_t         palette_offset;
  u16_t        transparency_rgb8;
  int          clip_x1;
  int          clip_x2;
  int          clip_y1;
  int          clip_y2;
  u8_t         offset_y;
  u16_t        offset_x;
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

  self.clip_x1 = 0;
  self.clip_x2 = 255;
  self.clip_y1 = 0;
  self.clip_y2 = 191;
}


u8_t layer2_access_read(void) {
  return (self.mapping << 6) | self.do_map_shadow | self.is_readable | self.is_visible | self.is_writable;

}


void layer2_access_write(u8_t value) {
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

    memory_refresh_accessors(0, 6);
  }
}


void layer2_control_write(u8_t value) {
  self.resolution     = (value & 0x30) >> 4;
  self.palette_offset = value & 0x0F;
}


void layer2_active_bank_write(u8_t bank) {
  if (bank != self.active_bank) {
    self.active_bank = bank;
  }
}


void layer2_shadow_bank_write(u8_t bank) {
  if (bank != self.shadow_bank) {
    self.shadow_bank = bank;
  }
}


void layer2_tick(u32_t row, u32_t column, int* is_enabled, const palette_entry_t** rgb, int* is_priority) {
  u8_t palette_index ;

  if (!self.is_visible) {
    *is_enabled = 0;
    return;
  }

  switch (self.resolution) {
    case E_RESOLUTION_256X192:
      if (row < 32 || row >= 32 + 192 || column < 32 * 2 || column >= (32 + 256) * 2) {
        *is_enabled = 0;
        return;
      }

      /* Convert to interior 256x192 space. */
      row    = row - 32;
      column = (column - 32 * 2) / 2;

      /* Honour the clipping area. */
      if (row     < self.clip_y1
       || row     > self.clip_y2
       || column  < self.clip_x1
       || column  > self.clip_x2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + self.offset_y) % 192;
      column = (column + self.offset_x) % 256;

      palette_index = self.ram[self.active_bank * 16 * 1024 + row * 256 + column];
      break;

    case E_RESOLUTION_320X256:
      column /= 2;

      /* Honour the clipping area. */
      if (row        < self.clip_y1
       || row        > self.clip_y2
       || column     < self.clip_x1 * 2
       || column     > self.clip_x2 * 2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + self.offset_y) % 256;
      column = (column + self.offset_x) % 320;

      palette_index = self.ram[self.active_bank * 16 * 1024 + column * 256 + row];
      break;

    default:
      /* Honour the clipping area. */
      if (row        < self.clip_y1
       || row        > self.clip_y2
       || column / 2 < self.clip_x1
       || column / 2 > self.clip_x2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + self.offset_y) % 256;
      column = (column + self.offset_x) % 640;

      palette_index = self.ram[self.active_bank * 16 * 1024 + column / 2 * 256 + row];
      palette_index = (column & 1) ? (palette_index & 0x0F) : (palette_index >> 4);
      break;
  }

  *is_enabled  = 1;
  *rgb         = palette_read(self.palette, (self.palette_offset << 4) + palette_index);
  *is_priority = (*rgb)->is_layer2_priority;
}


int layer2_is_readable(int page) {
  if (!self.is_readable) {
    return 0;
  }

  if (page < 2) {
    /* First 16K. */
    return 1;
  }

  if (page < 6) {
    /* First 48K. */
    return (self.mapping == E_MAPPING_FIRST_48K);
  }

  return 0;
}


int layer2_is_writable(int page) {
  if (!self.is_writable) {
    return 0;
  }

  if (page < 2) {
    /* First 16K. */
    return 1;
  }

  if (page < 6) {
    /* First 48K. */
    return (self.mapping == E_MAPPING_FIRST_48K);
  }

  return 0;
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


void layer2_palette_set(int use_second) {
  self.palette = use_second ? E_PALETTE_LAYER2_SECOND : E_PALETTE_LAYER2_FIRST;
}


void layer2_transparency_colour_write(u8_t rgb) {
  self.transparency_rgb8 = rgb;
}


void layer2_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;
}


void layer2_offset_x_msb_write(u8_t value) {
  self.offset_x = (value << 8) | (self.offset_x & 0x00FF);
}


void layer2_offset_x_lsb_write(u8_t value) {
  self.offset_x = (self.offset_x & 0xFF00) | value;
}


void layer2_offset_y_write(u8_t value) {
  self.offset_y = value;
}
