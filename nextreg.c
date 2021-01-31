#include "altrom.h"
#include "audio.h"
#include "ay.h"
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "cpu.h"
#include "defs.h"
#include "io.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "palette.h"
#include "paging.h"
#include "slu.h"
#include "rom.h"
#include "ula.h"


#define MACHINE_ID              8  /* Emulator. */

#define CORE_VERSION_MAJOR      3
#define CORE_VERSION_MINOR      1
#define CORE_VERSION_SUB_MINOR  9


typedef enum {
  E_NEXTREG_AY_1 = 0,
  E_NEXTREG_AY_2,
  E_NEXTREG_AY_3
} nextreg_ay_t;


typedef enum {
  E_NEXTREG_AY_STEREO_MODE_ABC,
  E_NEXTREG_AY_STEREO_MODE_ACB
} nextreg_ay_stereo_mode_t;


typedef struct {
  u8_t                     selected_register;
  int                      is_hard_reset;
  int                      palette_disable_auto_increment;
  palette_t                palette_sprites;
  palette_t                palette_layer2;
  palette_t                palette_ula;
  palette_t                palette_selected;
  u8_t                     palette_index;
  int                      palette_index_9bit_is_first_write;
  u8_t                     fallback_colour;
  int                      ula_next_mode;
  u8_t                     ula_clip_index;
  u8_t                     ula_clip[4];  /* Elements: x1, x2, y1, y2. */
  int                      altrom_soft_reset_enable;
  int                      altrom_soft_reset_during_writes;
  u8_t                     altrom_soft_reset_lock;
  nextreg_ay_stereo_mode_t ay_stereo_mode;
  int                      is_ay_mono[3];
  int                      is_hotkey_cpu_speed_enabled;
  int                      is_hotkey_nmi_multiface_enabled;
  int                      is_hotkey_nmi_divmmc_enabled;
} nextreg_t;


static nextreg_t self;


static void nextreg_cpu_speed_write(u8_t value);
static void nextreg_layer2_active_ram_bank_write(u8_t value);
static void nextreg_layer2_shadow_ram_bank_write(u8_t value);


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
  self.ula_clip[0]                       = 0;
  self.ula_clip[1]                       = 255;
  self.ula_clip[2]                       = 0;
  self.ula_clip[3]                       = 191;
  self.is_hotkey_cpu_speed_enabled       = 1;

  ula_clip_set(self.ula_clip[0], self.ula_clip[1], self.ula_clip[2], self.ula_clip[3]);
  ula_palette_set(self.palette_ula == E_PALETTE_ULA_SECOND);
  ula_contention_set(1);

  ay_reset();
  io_reset();
  mmu_reset();
  paging_reset();

  rom_select(0);
  rom_lock(self.altrom_soft_reset_lock);
  altrom_activate(self.altrom_soft_reset_enable, self.altrom_soft_reset_during_writes);

  nextreg_cpu_speed_write(E_CLOCK_CPU_SPEED_3MHZ);
  nextreg_layer2_active_ram_bank_write(8);
  nextreg_layer2_shadow_ram_bank_write(11);
}


static void nextreg_ay_configure(nextreg_ay_t ay) {
  const audio_channel_t a      =  self.is_ay_mono[ay] ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_LEFT;
  const audio_channel_t b      = (self.is_ay_mono[ay] || self.ay_stereo_mode == E_NEXTREG_AY_STEREO_MODE_ABC) ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_RIGHT;
  const audio_channel_t c      = (self.is_ay_mono[ay] || self.ay_stereo_mode == E_NEXTREG_AY_STEREO_MODE_ACB) ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_RIGHT;
  const audio_source_t  source = E_AUDIO_SOURCE_AY_1_CHANNEL_A + ay * 3;

  audio_assign_channel(source + 0, a);
  audio_assign_channel(source + 1, b);
  audio_assign_channel(source + 2, c);
}


static void nextreg_reset_hard(void) {
  memset(&self, 0, sizeof(self));

  self.is_hard_reset                   = 1;
  self.altrom_soft_reset_during_writes = 1;
  self.ay_stereo_mode                  = E_NEXTREG_AY_STEREO_MODE_ABC;
  self.is_hotkey_nmi_multiface_enabled = 0;
  self.is_hotkey_nmi_divmmc_enabled    = 0;

  nextreg_reset_soft();

  bootrom_activate();
  config_set_rom_ram_bank(0);
  config_activate();

  nextreg_ay_configure(E_NEXTREG_AY_1);
  nextreg_ay_configure(E_NEXTREG_AY_2);
  nextreg_ay_configure(E_NEXTREG_AY_3);

  ula_timex_set(0);
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
  return self.selected_register;
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
#ifdef DEBUG
    const char* descriptions[8] = {
      "configuration mode",
      "ZX Spectrum 48K",
      "ZX Spectrum 128K/+2",
      "ZX Spectrum +2A/+2B/+3",
      "Pentagon"
    };
#endif
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


static u8_t nextreg_peripheral_2_setting_read(void) {
  return self.is_hotkey_cpu_speed_enabled     << 7
       | self.is_hotkey_nmi_divmmc_enabled    << 4
       | self.is_hotkey_nmi_multiface_enabled << 3;
}


static void nextreg_peripheral_2_setting_write(u8_t value) {
  self.is_hotkey_cpu_speed_enabled     = (value & 0x80) >> 7;
  self.is_hotkey_nmi_divmmc_enabled    = (value & 0x10) >> 4;
  self.is_hotkey_nmi_multiface_enabled = (value & 0x08) >> 3;

}


static u8_t nextreg_peripheral_3_setting_read(void) {
  return !paging_spectrum_128k_paging_is_locked() << 7
       | !ula_contention_get() << 6
       | (self.ay_stereo_mode == E_NEXTREG_AY_STEREO_MODE_ACB) << 5;
}


static void nextreg_peripheral_3_setting_write(u8_t value) {
  const nextreg_ay_stereo_mode_t mode = value & 0x20 ? E_NEXTREG_AY_STEREO_MODE_ACB : E_NEXTREG_AY_STEREO_MODE_ABC;

  if (mode != self.ay_stereo_mode) {
    self.ay_stereo_mode = mode;

    nextreg_ay_configure(E_NEXTREG_AY_1);
    nextreg_ay_configure(E_NEXTREG_AY_2);
    nextreg_ay_configure(E_NEXTREG_AY_3);
  }

  if (value & 0x80) {
    paging_spectrum_128k_paging_unlock();
  }

  ula_contention_set((value & 0x40) == 0);

  ula_timex_set((value & 0x02) != 0);
}


static u8_t nextreg_peripheral_4_setting_read(void) {
  return self.is_ay_mono[E_NEXTREG_AY_3] << 7
       | self.is_ay_mono[E_NEXTREG_AY_2] << 6
       | self.is_ay_mono[E_NEXTREG_AY_1] << 5;
}


static void nextreg_peripheral_4_setting_write(u8_t value) {
  self.is_ay_mono[E_NEXTREG_AY_1] = (value & 0x20) >> 5;
  self.is_ay_mono[E_NEXTREG_AY_2] = (value & 0x40) >> 6;
  self.is_ay_mono[E_NEXTREG_AY_3] = (value & 0x80) >> 7;

  nextreg_ay_configure(E_NEXTREG_AY_1);
  nextreg_ay_configure(E_NEXTREG_AY_2);
  nextreg_ay_configure(E_NEXTREG_AY_3);
}


static u8_t nextreg_cpu_speed_read(void) {
  const u8_t speed = clock_cpu_speed_get();

  return speed << 4 | speed;
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
    paging_spectrum_128k_ram_bank_slot_4_set(value >> 4);
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
  const u8_t rom           = (value & 0x30) >> 4;

  self.altrom_soft_reset_enable        = value & 0x08;
  self.altrom_soft_reset_during_writes = value & 0x04;
  self.altrom_soft_reset_lock          = value & 0x03;

  rom_lock(rom);
  altrom_activate(enable, during_writes);
}


static u8_t nextreg_clip_window_ula_read(void) {
  return self.ula_clip[self.ula_clip_index];
}


static void nextreg_clip_window_ula_write(u8_t value) {
  self.ula_clip[self.ula_clip_index] = value;

  if (++self.ula_clip_index == 4) {
    self.ula_clip_index = 0;
    ula_clip_set(self.ula_clip[0], self.ula_clip[1], self.ula_clip[2], self.ula_clip[3]);
  }
}


static u8_t nextreg_clip_window_control_read(void) {
  return self.ula_clip_index << 4;
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


static void nextreg_internal_port_decoding_0_write(u8_t value) {
  io_port_enable(0x7FFD, value & 0x02);
}


static void nextreg_internal_port_decoding_2_write(u8_t value) {
  io_port_enable(0xBFFD, value & 0x01);
  io_port_enable(0xFFFD, value & 0x01);
}


u8_t nextreg_layer2_active_ram_bank_read(void) {
  return layer2_bank_active_get();
}


static void nextreg_layer2_active_ram_bank_write(u8_t value) {
  layer2_bank_active_set(value & 0x7F);
}


u8_t nextreg_layer2_shadow_ram_bank_read(void) {
  return layer2_bank_shadow_get();
}


static void nextreg_layer2_shadow_ram_bank_write(u8_t value) {
  layer2_bank_shadow_set(value & 0x7F);
}


static u8_t nextreg_sprite_layers_system_read(void) {
  /* TODO: Implement other bits in this register. */
  return slu_layer_priority_get() << 2;
}


void nextreg_sprite_layers_system_write(u8_t value) {
  /* TODO: Implement other bits in this register. */
  slu_layer_priority_set((value & 0x1C) >> 2);
}


void nextreg_data_write(u16_t address, u8_t value) {
  log_dbg("nextreg: write of $%02X to $%04X (register $%02X)\n", value, address, self.selected_register);
  nextreg_write_internal(self.selected_register, value);
}


void nextreg_write_internal(u8_t reg, u8_t value) {
  switch (reg) {
    case E_NEXTREG_REGISTER_CONFIG_MAPPING:
      nextreg_config_mapping_write(value);
      break;

    case E_NEXTREG_REGISTER_RESET:
      nextreg_reset_write(value);
      break;

    case E_NEXTREG_REGISTER_MACHINE_TYPE:
      nextreg_machine_type_write(value);
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_1_SETTING:
      nextreg_peripheral_1_setting_write(value);
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_2_SETTING:
      nextreg_peripheral_2_setting_write(value);
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_3_SETTING:
      nextreg_peripheral_3_setting_write(value);
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_4_SETTING:
      nextreg_peripheral_4_setting_write(value);
      break;

    case E_NEXTREG_REGISTER_CPU_SPEED:
      nextreg_cpu_speed_write(value);
      break;

    case E_NEXTREG_REGISTER_VIDEO_TIMING:
      nextreg_video_timing_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_ACTIVE_RAM_BANK:
      nextreg_layer2_active_ram_bank_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_SHADOW_RAM_BANK:
      nextreg_layer2_shadow_ram_bank_write(value);
      break;

    case E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM:
      nextreg_sprite_layers_system_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_ULA:
      nextreg_clip_window_ula_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_CONTROL:
      nextreg_clip_window_control_write(value);
      break;

    case E_NEXTREG_REGISTER_PALETTE_INDEX:
      nextreg_palette_index_write(value);
      break;

    case E_NEXTREG_REGISTER_PALETTE_CONTROL:
      nextreg_palette_control_write(value);
      break;

    case E_NEXTREG_REGISTER_PALETTE_VALUE_8BITS:
      nextreg_palette_value_8bits_write(value);
      break;

    case E_NEXTREG_REGISTER_PALETTE_VALUE_9BITS:
      nextreg_palette_value_9bits_write(value);
      break;

    case E_NEXTREG_REGISTER_FALLBACK_COLOUR:
      self.fallback_colour = value;
      break;

    case E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL:
      mmu_page_set(self.selected_register - E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL, value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_0:
      nextreg_internal_port_decoding_0_write(value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_2:
      nextreg_internal_port_decoding_2_write(value);
      break;

    case E_NEXTREG_REGISTER_ALTERNATE_ROM:
      nextreg_alternate_rom_write(value);
      break;

    case E_NEXTREG_REGISTER_SPECTRUM_MEMORY_MAPPING:
      nextreg_spectrum_memory_mapping_write(value);
      break;

    default:
      log_wrn("nextreg: unimplemented write of $%02X to Next register $%02X\n", value, reg);
      break;
  }
}


u8_t nextreg_data_read(u16_t address) {
  log_dbg("nextreg: read from $%04X\n", address);
  return nextreg_read_internal(self.selected_register);
}


u8_t nextreg_read_internal(u8_t reg) {
  switch (reg) {
    case E_NEXTREG_REGISTER_MACHINE_ID:
      return MACHINE_ID;

    case E_NEXTREG_REGISTER_CORE_VERSION:
      return CORE_VERSION_MAJOR << 4 | CORE_VERSION_MINOR;

    case E_NEXTREG_REGISTER_CORE_VERSION_SUB_MINOR:
      return CORE_VERSION_SUB_MINOR;

    case E_NEXTREG_REGISTER_RESET:
      return nextreg_reset_read();

    case E_NEXTREG_REGISTER_CPU_SPEED:
      return nextreg_cpu_speed_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_2_SETTING:
      return nextreg_peripheral_2_setting_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_3_SETTING:
      return nextreg_peripheral_3_setting_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_4_SETTING:
      return nextreg_peripheral_4_setting_read();

    case E_NEXTREG_REGISTER_LAYER2_ACTIVE_RAM_BANK:
      return nextreg_layer2_active_ram_bank_read();

    case E_NEXTREG_REGISTER_LAYER2_SHADOW_RAM_BANK:
      return nextreg_layer2_shadow_ram_bank_read();

    case E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM:
      return nextreg_sprite_layers_system_read();

    case E_NEXTREG_REGISTER_CLIP_WINDOW_ULA:
      return nextreg_clip_window_ula_read();

    case E_NEXTREG_REGISTER_CLIP_WINDOW_CONTROL:
      return nextreg_clip_window_control_read();

    case E_NEXTREG_REGISTER_PALETTE_INDEX:
      return self.palette_index;

    case E_NEXTREG_REGISTER_PALETTE_CONTROL:
      return nextreg_palette_control_read();

    case E_NEXTREG_REGISTER_PALETTE_VALUE_8BITS:
      return nextreg_palette_value_8bits_read();

    case E_NEXTREG_REGISTER_PALETTE_VALUE_9BITS:
      return nextreg_palette_value_9bits_read();

    case E_NEXTREG_REGISTER_FALLBACK_COLOUR:
      return self.fallback_colour;

    case E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL:
      return mmu_page_get(self.selected_register - E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL);

    default:
      log_wrn("nextreg: unimplemented read from Next register $%02X\n", reg);
      break;
  }

  return 0xFF;
}


