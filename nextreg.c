#include "altrom.h"
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "cpu.h"
#include "defs.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "palette.h"
#include "rom.h"
#include "ula.h"


#define MACHINE_ID              8  /* Emulator. */

#define CORE_VERSION_MAJOR      3
#define CORE_VERSION_MINOR      1
#define CORE_VERSION_SUB_MINOR  9

/* See https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt */
#define REGISTER_MACHINE_ID              0x00
#define REGISTER_CORE_VERSION            0x01
#define REGISTER_RESET                   0x02
#define REGISTER_MACHINE_TYPE            0x03
#define REGISTER_CONFIG_MAPPING          0x04
#define REGISTER_PERIPHERAL_1_SETTING    0x05
#define REGISTER_PERIPHERAL_2_SETTING    0x06
#define REGISTER_CPU_SPEED               0x07
#define REGISTER_PERIPHERAL_3_SETTING    0x08
#define REGISTER_PERIPHERAL_4_SETTING    0x09
#define REGISTER_CORE_VERSION_SUB_MINOR  0x0E
#define REGISTER_PERIPHERAL_5_SETTING    0x10
#define REGISTER_VIDEO_TIMING            0x11
#define REGISTER_CLIP_WINDOW_ULA         0x1A
#define REGISTER_CLIP_WINDOW_CONTROL     0x1C
#define REGISTER_PALETTE_INDEX           0x40
#define REGISTER_PALETTE_VALUE_8BITS     0x41
#define REGISTER_PALETTE_CONTROL         0x43
#define REGISTER_PALETTE_VALUE_9BITS     0x44
#define REGISTER_FALLBACK_COLOUR         0x4A
#define REGISTER_MMU_SLOT0_CONTROL       0x50
#define REGISTER_MMU_SLOT1_CONTROL       0x51
#define REGISTER_MMU_SLOT2_CONTROL       0x52
#define REGISTER_MMU_SLOT3_CONTROL       0x53
#define REGISTER_MMU_SLOT4_CONTROL       0x54
#define REGISTER_MMU_SLOT5_CONTROL       0x55
#define REGISTER_MMU_SLOT6_CONTROL       0x56
#define REGISTER_MMU_SLOT7_CONTROL       0x57
#define REGISTER_ALTERNATE_ROM           0x8C
#define REGISTER_SPECTRUM_MEMORY_MAPPING 0x8E
#define REGISTER_MEMORY_MAPPING_MODE     0x8F


typedef struct {
  u8_t      selected_register;
  int       is_hard_reset;
  int       palette_disable_auto_increment;
  palette_t palette_sprites;
  palette_t palette_layer2;
  palette_t palette_ula;
  palette_t palette_selected;
  u8_t      palette_index;
  int       palette_index_9bit_is_first_write;
  u8_t      fallback_colour;
  int       ula_next_mode;
  int       ula_clip_index;
  int       ula_clip_x1;
  int       ula_clip_x2;
  int       ula_clip_y1;
  int       ula_clip_y2;
} nextreg_t;


static nextreg_t self;


static void nextreg_reset_soft(void) {
  self.palette_disable_auto_increment    = 0;
  self.palette_selected                  = E_PALETTE_ULA_FIRST;
  self.palette_index                     = 0;
  self.palette_index_9bit_is_first_write = 1;
  self.palette_sprites                   = E_PALETTE_SPRITES_FIRST;
  self.palette_layer2                    = E_PALETTE_LAYER2_FIRST;
  self.palette_ula                       = E_PALETTE_ULA_FIRST;
  self.fallback_colour                   = 0xE3;
  self.ula_next_mode                     = 0;
  self.ula_clip_index                    = 0;
  self.ula_clip_x1                       = 0;
  self.ula_clip_x2                       = 159;
  self.ula_clip_y1                       = 0;
  self.ula_clip_y2                       = 255;

  ula_clip_set(self.ula_clip_x1, self.ula_clip_x2, self.ula_clip_y1, self.ula_clip_y2);
  ula_palette_set(self.palette_ula == E_PALETTE_ULA_SECOND);
}


static void nextreg_reset_hard(void) {
  self.selected_register = 0x00;
  self.is_hard_reset     = 1;

  nextreg_reset_soft();

  bootrom_activate();
  config_activate();
}


int nextreg_init(void) {
  nextreg_reset_hard();
  return 0;
}


void nextreg_finit(void) {
}


void nextreg_select_write(u16_t address, u8_t value) {
  self.selected_register = value;
}


u8_t nextreg_select_read(u16_t address) {
  return 0xFF;
}


static u8_t nextreg_reset_read(void) {
  return self.is_hard_reset ? 0x02 : 0x01;
}


static void nextreg_reset_write(u8_t value) {
  if (value & 0x03) {
    /* Hard or soft reset. */
    self.is_hard_reset = value & 0x02;
    if (self.is_hard_reset) {
      nextreg_reset_hard();
    } else {
      nextreg_reset_soft();
    }

    cpu_reset();

    log_dbg("nextreg: %s reset\n", self.is_hard_reset ? "hard" : "soft");
  }
}


static void nextreg_config_mapping_write(u8_t value) {
  /* Only bits 4:0 are specified, but FPGA uses bits 6:0. */
  config_set_rom_ram_bank(value & 0x7F);
}


static void nextreg_machine_type_write(u8_t value) {
  if (value & 0x80) {
    const u8_t display_timing = (value >> 4) & 0x03;
    if (display_timing <= E_ULA_DISPLAY_TIMING_PENTAGON) {
      ula_display_timing_set(display_timing);
    }
  }

  if (config_is_active()) {
    const char* descriptions[8] = {
      "configuration mode",
      "ZX Spectrum 48K",
      "ZX Spectrum 128K/+2",
      "ZX Spectrum +2A/+2B/+3",
      "Pentagon"
    };
    u8_t machine_type = value & 0x03;

    if (machine_type > E_MACHINE_TYPE_PENTAGON) {
      machine_type = E_MACHINE_TYPE_CONFIG_MODE;
    }

    log_dbg("nextreg: machine type set to %s\n", descriptions[machine_type]);

    bootrom_deactivate();

    if (machine_type == E_MACHINE_TYPE_CONFIG_MODE) {
      config_activate();
    } else {
      config_deactivate();
    }

    rom_set_machine_type(machine_type);
  }
}


static void nextreg_peripheral_1_setting_write(u8_t value) {
  ula_display_frequency_set((value & 0x04) >> 2);
}


static void nextreg_cpu_speed_write(u8_t value) {
  clock_cpu_speed_set(value & 0x03);
}


static void nextreg_video_timing_write(u8_t value) {
  if (config_is_active()) {
    clock_video_timing_set(value & 0x03);
  }
}


static void nextreg_memory_all_ram(u8_t slot_1_bank_16k, u8_t slot_2_bank_16k, u8_t slot_3_bank_16k, u8_t slot_4_bank_16k) {
  mmu_bank_set(1, slot_1_bank_16k);
  mmu_bank_set(2, slot_2_bank_16k);
  mmu_bank_set(3, slot_3_bank_16k);
  mmu_bank_set(4, slot_4_bank_16k);
}


static void nextreg_spectrum_memory_mapping_write(u8_t value) {
  const int change_bank = value & 0x08;
  const int paging_mode = value & 0x04;
  
  if (change_bank) {
    mmu_bank_set(4, value >> 4);
  }

  if (paging_mode == 0) {
    rom_select(value & 0x03);
  } else {
    switch (value & 0x03) {
      case 0: nextreg_memory_all_ram(0, 1, 2, 3); break;
      case 1: nextreg_memory_all_ram(4, 5, 6, 7); break;
      case 2: nextreg_memory_all_ram(4, 5, 6, 3); break;
      case 3: nextreg_memory_all_ram(4, 7, 6, 3); break;
    }
  }
}


static void nextreg_alternate_rom_write(u8_t value) {
  const int  enable        = value & 0x80;
  const int  during_writes = value & 0x40;
  const u8_t lock          = (value & 0x30) >> 4;

  rom_set_lock(lock);
  altrom_activate(enable, during_writes);
}


static void nextreg_clip_window_ula_write(u8_t value) {
  switch (self.ula_clip_index++) {
    case 0:
      self.ula_clip_x1 = value * 2;
      break;

    case 1:
      self.ula_clip_x2 = value * 2;
      break;

    case 2:
      self.ula_clip_y1 = value;
      break;

    case 3:
      self.ula_clip_y2 = value;
      ula_clip_set(self.ula_clip_x1, self.ula_clip_x2, self.ula_clip_y1, self.ula_clip_y2);
      break;
  }

  self.ula_clip_index &= 0x03;
}


static void nextreg_clip_window_control_write(u8_t value) {
  if (value & 0x04) {
    self.ula_clip_index = 0;
  }
}


static u8_t nextreg_palette_control_read(void) {
  return self.palette_disable_auto_increment                << 7
       | self.palette_selected                              << 4
       | (self.palette_sprites == E_PALETTE_SPRITES_SECOND) << 3
       | (self.palette_layer2  == E_PALETTE_LAYER2_SECOND)  << 2
       | (self.palette_ula     == E_PALETTE_ULA_SECOND)     << 1
       | self.ula_next_mode;
}


static void nextreg_palette_control_write(u8_t value) {
  self.palette_disable_auto_increment = value >> 7;
  self.palette_selected               = (value & 0x70) >> 4;
  self.palette_sprites                = (value & 0x08) ? E_PALETTE_SPRITES_SECOND : E_PALETTE_SPRITES_FIRST;
  self.palette_layer2                 = (value & 0x04) ? E_PALETTE_LAYER2_SECOND  : E_PALETTE_LAYER2_FIRST;
  self.palette_ula                    = (value & 0x02) ? E_PALETTE_ULA_SECOND     : E_PALETTE_ULA_FIRST;
  self.ula_next_mode                  = value & 0x01;

  ula_palette_set(self.palette_ula == E_PALETTE_ULA_SECOND);

  if (self.ula_next_mode) {
    log_wrn("nextreg: ULANext mode not implemented\n");
  }
}


static void nextreg_palette_index_write(u8_t value) {
  self.palette_index                     = value;
  self.palette_index_9bit_is_first_write = 1;

  log_dbg("nextreg: palette index set to %u\n", self.palette_index);
}


static u8_t nextreg_palette_value_8bits_read(void) {
  const palette_entry_t entry = palette_read(self.palette_selected, self.palette_index);

  return entry.red << 5 | entry.green << 2 | entry.blue >> 1;
}


static void nextreg_palette_value_8bits_write(u8_t value) {
  const palette_entry_t entry = {
    .red                = (value & 0xE0) >> 5,
    .green              = (value & 0x1C) >> 2,
    .blue               = (value & 0x03) << 1 | ((value & 0x03) != 0),
    .is_layer2_priority = 0
  };
  palette_write(self.palette_selected, self.palette_index, entry);

  if (!self.palette_disable_auto_increment) {
    self.palette_index++;
  }
}


static u8_t nextreg_palette_value_9bits_read(void) {
  const palette_entry_t entry = palette_read(self.palette_selected, self.palette_index);

  return entry.is_layer2_priority << 7 | (entry.blue & 0x01);
}


static void nextreg_palette_value_9bits_write(u8_t value) {
  if (self.palette_index_9bit_is_first_write) {
    const palette_entry_t entry = {
      .red                = (value & 0xE0) >> 5,
      .green              = (value & 0x1C) >> 2,
      .blue               = (value & 0x03) << 1,
      .is_layer2_priority = 0
    };
    palette_write(self.palette_selected, self.palette_index, entry);
  } else {
    palette_entry_t entry = palette_read(self.palette_selected, self.palette_index);
    entry.blue              |= value & 0x01;
    entry.is_layer2_priority = value >> 7;
    palette_write(self.palette_selected, self.palette_index, entry);

    if (!self.palette_disable_auto_increment) {
      self.palette_index++;
    }
  }

  self.palette_index_9bit_is_first_write ^= 1;  
}


void nextreg_data_write(u16_t address, u8_t value) {
  switch (self.selected_register) {
    case REGISTER_CONFIG_MAPPING:
      nextreg_config_mapping_write(value);
      break;

    case REGISTER_RESET:
      nextreg_reset_write(value);
      break;

    case REGISTER_MACHINE_TYPE:
      nextreg_machine_type_write(value);
      break;

    case REGISTER_PERIPHERAL_1_SETTING:
      nextreg_peripheral_1_setting_write(value);
      break;

    case REGISTER_CPU_SPEED:
      nextreg_cpu_speed_write(value);
      break;

    case REGISTER_VIDEO_TIMING:
      nextreg_video_timing_write(value);
      break;

    case REGISTER_CLIP_WINDOW_ULA:
      nextreg_clip_window_ula_write(value);
      break;

    case REGISTER_CLIP_WINDOW_CONTROL:
      nextreg_clip_window_control_write(value);
      break;

    case REGISTER_PALETTE_INDEX:
      nextreg_palette_index_write(value);
      break;

    case REGISTER_PALETTE_CONTROL:
      nextreg_palette_control_write(value);
      break;

    case REGISTER_PALETTE_VALUE_8BITS:
      nextreg_palette_value_8bits_write(value);
      break;

    case REGISTER_PALETTE_VALUE_9BITS:
      nextreg_palette_value_9bits_write(value);
      break;

    case REGISTER_FALLBACK_COLOUR:
      self.fallback_colour = value;
      break;

    case REGISTER_MMU_SLOT0_CONTROL:
    case REGISTER_MMU_SLOT1_CONTROL:
    case REGISTER_MMU_SLOT2_CONTROL:
    case REGISTER_MMU_SLOT3_CONTROL:
    case REGISTER_MMU_SLOT4_CONTROL:
    case REGISTER_MMU_SLOT5_CONTROL:
    case REGISTER_MMU_SLOT6_CONTROL:
    case REGISTER_MMU_SLOT7_CONTROL:
      mmu_page_set(self.selected_register - REGISTER_MMU_SLOT0_CONTROL, value);
      break;

    case REGISTER_ALTERNATE_ROM:
      nextreg_alternate_rom_write(value);
      break;

    case REGISTER_SPECTRUM_MEMORY_MAPPING:
      nextreg_spectrum_memory_mapping_write(value);
      break;

    default:
      log_wrn("nextreg: unimplemented write of $%02X to Next register $%02X\n", value, self.selected_register);
      break;
  }
}


u8_t nextreg_data_read(u16_t address) {
  switch (self.selected_register) {
    case REGISTER_MACHINE_ID:
      return MACHINE_ID;

    case REGISTER_CORE_VERSION:
      return CORE_VERSION_MAJOR << 4 | CORE_VERSION_MINOR;

    case REGISTER_CORE_VERSION_SUB_MINOR:
      return CORE_VERSION_SUB_MINOR;

    case REGISTER_RESET:
      return nextreg_reset_read();

    case REGISTER_PALETTE_INDEX:
      return self.palette_index;

    case REGISTER_PALETTE_CONTROL:
      return nextreg_palette_control_read();

    case REGISTER_PALETTE_VALUE_8BITS:
      return nextreg_palette_value_8bits_read();

    case REGISTER_PALETTE_VALUE_9BITS:
      return nextreg_palette_value_9bits_read();

    case REGISTER_FALLBACK_COLOUR:
      return self.fallback_colour;

    case REGISTER_MMU_SLOT0_CONTROL:
    case REGISTER_MMU_SLOT1_CONTROL:
    case REGISTER_MMU_SLOT2_CONTROL:
    case REGISTER_MMU_SLOT3_CONTROL:
    case REGISTER_MMU_SLOT4_CONTROL:
    case REGISTER_MMU_SLOT5_CONTROL:
    case REGISTER_MMU_SLOT6_CONTROL:
    case REGISTER_MMU_SLOT7_CONTROL:
      return mmu_page_get(self.selected_register - REGISTER_MMU_SLOT0_CONTROL);

    default:
      log_wrn("nextreg: unimplemented read from Next register $%02X\n", self.selected_register);
      break;
  }

  return 0xFF;
}


