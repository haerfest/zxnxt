#include <string.h>
#include "defs.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "palette.h"


typedef enum resolution_t {
  E_RESOLUTION_256X192 = 0,
  E_RESOLUTION_320X256,
  E_RESOLUTION_640X256
} resolution_t;


typedef enum mapping_t {
  E_MAPPING_FIRST_16K = 0,
  E_MAPPING_SECOND_16K,
  E_MAPPING_THIRD_16K,
  E_MAPPING_FIRST_48K
} mapping_t;


typedef struct layer2_t {
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
  int          clip_x1;
  int          clip_x2;
  int          clip_y1;
  int          clip_y2;
  u8_t         offset_y;
  u16_t        offset_x;
} layer2_t;


static layer2_t layer2;


int layer2_init(u8_t* sram) {
  memset(&layer2, 0, sizeof(layer2));

  layer2.ram = &sram[MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM];

  layer2_reset(E_RESET_HARD);

  return 0;
}


void layer2_finit(void) {
}


void layer2_reset(reset_t reset) {
  layer2_access_write(0x00);
  layer2_access_write(0x10);
  layer2_active_bank_write(8);
  layer2_shadow_bank_write(11);

  layer2.clip_x1       = 0;
  layer2.clip_x2       = 255;
  layer2.clip_y1       = 0;
  layer2.clip_y2       = 191;
  layer2.offset_x      = 0;
  layer2.offset_y      = 0;
  layer2.palette       = E_PALETTE_LAYER2_FIRST;
  layer2.resolution    = E_RESOLUTION_256X192;
  layer2.is_visible    = 0;
  layer2.is_readable   = 0;
  layer2.is_writable   = 0;
  layer2.mapping       = E_MAPPING_FIRST_16K;
  layer2.do_map_shadow = 0;
  layer2.bank_offset   = 0;

  memory_refresh_accessors(0, 6);
}


u8_t layer2_access_read(void) {
  return (layer2.mapping << 6) | layer2.do_map_shadow | layer2.is_readable | layer2.is_visible | layer2.is_writable;

}


void layer2_access_write(u8_t value) {
  if (value & 0x10)
  {
    layer2.bank_offset = value & 0x07;
  }
  else
  {
    layer2.mapping       = value >> 6;
    layer2.do_map_shadow = value & 0x08;
    layer2.is_readable   = value & 0x04;
    layer2.is_visible    = value & 0x02;
    layer2.is_writable   = value & 0x01;

    memory_refresh_accessors(0, 6);
  }
}


void layer2_control_write(u8_t value) {
  layer2.resolution     = (value & 0x30) >> 4;
  layer2.palette_offset = value & 0x0F;
}


u8_t layer2_active_bank_read(void) {
  return layer2.active_bank;
}


void layer2_active_bank_write(u8_t bank) {
  if (bank != layer2.active_bank) {
    layer2.active_bank = bank;
  }
}


u8_t layer2_shadow_bank_read(void) {
  return layer2.shadow_bank;
}


void layer2_shadow_bank_write(u8_t bank) {
  if (bank != layer2.shadow_bank) {
    layer2.shadow_bank = bank;
  }
}


inline
static void layer2_tick(u32_t row, u32_t column, int* is_enabled, const palette_entry_t** rgb, int* is_priority) {
  u8_t palette_index ;

  if (!layer2.is_visible) {
    *is_enabled = 0;
    return;
  }

  switch (layer2.resolution) {
    case E_RESOLUTION_256X192:
      if (row < 32 || row >= 32 + 192 || column < 32 * 2 || column >= (32 + 256) * 2) {
        *is_enabled = 0;
        return;
      }

      /* Convert to interior 256x192 space. */
      row    = row - 32;
      column = (column - 32 * 2) / 2;

      /* Honour the clipping area. */
      if (row     < layer2.clip_y1
       || row     > layer2.clip_y2
       || column  < layer2.clip_x1
       || column  > layer2.clip_x2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + layer2.offset_y) % 192;
      column = (column + layer2.offset_x) % 256;

      palette_index = layer2.ram[layer2.active_bank * 16 * 1024 + row * 256 + column];
      break;

    case E_RESOLUTION_320X256:
      column /= 2;

      /* Honour the clipping area. */
      if (row        < layer2.clip_y1
       || row        > layer2.clip_y2
       || column     < layer2.clip_x1 * 2
       || column     > layer2.clip_x2 * 2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + layer2.offset_y) % 256;
      column = (column + layer2.offset_x) % 320;

      palette_index = layer2.ram[layer2.active_bank * 16 * 1024 + column * 256 + row];
      break;

    default:
      /* Honour the clipping area. */
      if (row        < layer2.clip_y1
       || row        > layer2.clip_y2
       || column / 2 < layer2.clip_x1
       || column / 2 > layer2.clip_x2) {
        *is_enabled = 0;
        return;
      }

      row    = (row    + layer2.offset_y) % 256;
      column = (column + layer2.offset_x) % 640;

      palette_index = layer2.ram[layer2.active_bank * 16 * 1024 + column / 2 * 256 + row];
      palette_index = (column & 1) ? (palette_index & 0x0F) : (palette_index >> 4);
      break;
  }

  *is_enabled  = 1;
  *rgb         = palette_read_inline(layer2.palette, (layer2.palette_offset << 4) + palette_index);
  *is_priority = (*rgb)->is_layer2_priority;
}


int layer2_is_readable(int page) {
  if (!layer2.is_readable) {
    return 0;
  }

  if (page < 2) {
    /* First 16K. */
    return 1;
  }

  if (page < 6) {
    /* First 48K. */
    return (layer2.mapping == E_MAPPING_FIRST_48K);
  }

  return 0;
}


int layer2_is_writable(int page) {
  if (!layer2.is_writable) {
    return 0;
  }

  if (page < 2) {
    /* First 16K. */
    return 1;
  }

  if (page < 6) {
    /* First 48K. */
    return (layer2.mapping == E_MAPPING_FIRST_48K);
  }

  return 0;
}


static u32_t layer2_translate(u16_t address) {
  const u8_t bank = (layer2.do_map_shadow ? layer2.shadow_bank : layer2.active_bank) + layer2.bank_offset;

  switch (layer2.mapping) {
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
  return layer2.ram[layer2_translate(address)];
}


void layer2_write(u16_t address, u8_t value) {
  layer2.ram[layer2_translate(address)] = value;
}


void layer2_palette_set(int use_second) {
  layer2.palette = use_second ? E_PALETTE_LAYER2_SECOND : E_PALETTE_LAYER2_FIRST;
}


void layer2_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  layer2.clip_x1 = x1;
  layer2.clip_x2 = x2;
  layer2.clip_y1 = y1;
  layer2.clip_y2 = y2;
}


void layer2_offset_x_msb_write(u8_t value) {
  layer2.offset_x = (value << 8) | (layer2.offset_x & 0x00FF);
}


void layer2_offset_x_lsb_write(u8_t value) {
  layer2.offset_x = (layer2.offset_x & 0xFF00) | value;
}


void layer2_offset_y_write(u8_t value) {
  layer2.offset_y = value;
}


void layer2_enable(int enable) {
  layer2.is_visible = enable;
}
