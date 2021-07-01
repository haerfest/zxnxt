#include "altrom.h"
#include "audio.h"
#include "ay.h"
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "copper.h"
#include "cpu.h"
#include "dac.h"
#include "defs.h"
#include "dma.h"
#include "i2c.h"
#include "io.h"
#include "keyboard.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "palette.h"
#include "paging.h"
#include "slu.h"
#include "sprites.h"
#include "tilemap.h"
#include "rom.h"
#include "uart.h"
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
  u8_t index;
  u8_t values[4];  /* x1, x2, y1, y2. */
} clip_t;


typedef struct {
  u8_t                     registers[256];
  u8_t                     selected_register;
  int                      is_hard_reset;
  int                      palette_disable_auto_increment;
  int                      is_palette_sprites_second;
  int                      is_palette_layer2_second;
  int                      is_palette_ula_second;
  palette_t                palette_selected;
  u8_t                     palette_index;
  int                      palette_index_9bit_is_first_write;
  int                      ula_next_mode;
  clip_t                   ula_clip;
  clip_t                   tilemap_clip;
  clip_t                   sprites_clip;
  clip_t                   layer2_clip;
  int                      altrom_soft_reset_enable;
  int                      altrom_soft_reset_during_writes;
  u8_t                     altrom_soft_reset_lock;
  nextreg_ay_stereo_mode_t ay_stereo_mode;
  int                      is_ay_mono[3];
  int                      is_hotkey_cpu_speed_enabled;
  int                      is_hotkey_nmi_multiface_enabled;
  int                      is_hotkey_nmi_divmmc_enabled;
  u8_t                     sprite_number;
  int                      is_sprites_lockstepped;
} nextreg_t;


static nextreg_t self;


static void nextreg_cpu_speed_write(u8_t value);


static void nextreg_ay_configure(nextreg_ay_t ay) {
  const audio_channel_t a      =  self.is_ay_mono[ay] ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_LEFT;
  const audio_channel_t b      = (self.is_ay_mono[ay] || self.ay_stereo_mode == E_NEXTREG_AY_STEREO_MODE_ABC) ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_RIGHT;
  const audio_channel_t c      = (self.is_ay_mono[ay] || self.ay_stereo_mode == E_NEXTREG_AY_STEREO_MODE_ACB) ? E_AUDIO_CHANNEL_BOTH : E_AUDIO_CHANNEL_RIGHT;
  const audio_source_t  source = E_AUDIO_SOURCE_AY_1_CHANNEL_A + ay * 3;

  audio_assign_channel(source + 0, a);
  audio_assign_channel(source + 1, b);
  audio_assign_channel(source + 2, c);
}



static void nextreg_reset(reset_t reset) {
  if (reset == E_RESET_HARD) {
    memset(&self, 0, sizeof(self));

    self.is_hard_reset                   = 1;
    self.altrom_soft_reset_during_writes = 1;
    self.ay_stereo_mode                  = E_NEXTREG_AY_STEREO_MODE_ABC;
    self.is_hotkey_nmi_multiface_enabled = 0;
    self.is_hotkey_nmi_divmmc_enabled    = 0;

    bootrom_activate();
    config_set_rom_ram_bank(0);
    config_activate();

    nextreg_ay_configure(E_NEXTREG_AY_1);
    nextreg_ay_configure(E_NEXTREG_AY_2);
    nextreg_ay_configure(E_NEXTREG_AY_3);
  }

  /* Both hard and soft resets. */
  self.palette_disable_auto_increment    = 0;
  self.palette_selected                  = E_PALETTE_ULA_FIRST;
  self.palette_index                     = 0;
  self.palette_index_9bit_is_first_write = 1;
  self.is_palette_sprites_second         = 0;
  self.is_palette_layer2_second          = 0;
  self.is_palette_ula_second             = 0;
  self.ula_next_mode                     = 0;
  self.is_hotkey_cpu_speed_enabled       = 1;

  ay_reset(reset);
  copper_reset(reset);
  dac_reset(reset);
  dma_reset(reset);
  i2c_reset(reset);
  io_reset(reset);
  layer2_reset(reset);
  mmu_reset(reset);
  paging_reset(reset);
  slu_reset(reset);
  sprites_reset(reset);
  tilemap_reset(reset);
  uart_reset(reset);
  ula_reset(reset);

  rom_select(0);
  rom_lock(self.altrom_soft_reset_lock);
  altrom_activate(self.altrom_soft_reset_enable, self.altrom_soft_reset_during_writes);

  nextreg_cpu_speed_write(E_CLOCK_CPU_SPEED_3MHZ);
}


int nextreg_init(void) {
  nextreg_reset(E_RESET_HARD);
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


static void nextreg_reset_write(u8_t value) {
  if (value & 0x03) {
    /* Hard or soft reset. */
    self.is_hard_reset = value & 0x02;
    nextreg_reset(self.is_hard_reset ? E_RESET_HARD : E_RESET_SOFT);
    cpu_reset();
    return;
  }

  if (value & 0x08) {
    cpu_nmi(E_CPU_NMI_REASON_MF);
    return;
  }

  if (value & 0x04) {
    cpu_nmi(E_CPU_NMI_REASON_DIVMMC);
    return;
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
    u8_t machine_type = value & 0x03;

    if (machine_type > E_MACHINE_TYPE_PENTAGON) {
      machine_type = E_MACHINE_TYPE_CONFIG_MODE;
    }

    bootrom_deactivate();

    if (machine_type == E_MACHINE_TYPE_CONFIG_MODE) {
      config_activate();
    } else {
      config_deactivate();
    }

    rom_set_machine_type(machine_type);
  }
}


static u8_t nextreg_core_boot_read(void) {
  return keyboard_is_special_key_pressed(E_KEYBOARD_SPECIAL_KEY_DRIVE) << 1
       | keyboard_is_special_key_pressed(E_KEYBOARD_SPECIAL_KEY_NMI);
}


static u8_t nextreg_peripheral_1_setting_read(void) {
  return ula_60hz_get() ? 0x04 : 0x00;
}


static void nextreg_peripheral_1_setting_write(u8_t value) {
  ula_60hz_set(value & 0x04);
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
  dac_enable((value & 0x08) >> 3);
  ula_timex_video_mode_read_enable((value & 0x04) != 0);
}


static u8_t nextreg_peripheral_4_setting_read(void) {
  return self.is_ay_mono[E_NEXTREG_AY_3] << 7
       | self.is_ay_mono[E_NEXTREG_AY_2] << 6
       | self.is_ay_mono[E_NEXTREG_AY_1] << 5
       | self.is_sprites_lockstepped     << 4;
}


static void nextreg_peripheral_4_setting_write(u8_t value) {
  self.is_ay_mono[E_NEXTREG_AY_1] = (value & 0x20) >> 5;
  self.is_ay_mono[E_NEXTREG_AY_2] = (value & 0x40) >> 6;
  self.is_ay_mono[E_NEXTREG_AY_3] = (value & 0x80) >> 7;

  nextreg_ay_configure(E_NEXTREG_AY_1);
  nextreg_ay_configure(E_NEXTREG_AY_2);
  nextreg_ay_configure(E_NEXTREG_AY_3);

  self.is_sprites_lockstepped = (value & 0x10) >> 4;
}


static void nextreg_peripheral_5_setting_write(u8_t value) {
  if (!config_is_active()) {
    return;
  }

  switch (value >> 5) {
    case 0:
      /* Multiface +3. */
      io_mf_ports_set(0x3F, 0xBF);
      break;

    case 1:
      /* Multiface 128 v87.2. */
      io_mf_ports_set(0xBF, 0x3F);
      break;

    default:
      /* Multiface 128 v87.12 or Multiface 1. */
      io_mf_ports_set(0x9F, 0x1F);
      break;
  }
}


static u8_t nextreg_cpu_speed_read(void) {
  const u8_t speed = clock_cpu_speed_get();

  return speed << 4 | speed;
}


static void nextreg_cpu_speed_write(u8_t value) {
  clock_cpu_speed_set(value & 0x03);
}


static void nextreg_spectrum_memory_mapping_write(u8_t value) {
  const int change_bank = value & 0x08;
  const int paging_mode = value & 0x04;
  
  if (change_bank) {
    paging_spectrum_128k_ram_bank_slot_4_set(value >> 4);
  }

  if (paging_mode == 0) {
    rom_select(value & 0x03);
    paging_all_ram_disable();
  } else {
    paging_all_ram(value & 0x03);
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


static void nextreg_clip_window_ula_write(u8_t value) {
  self.ula_clip.values[self.ula_clip.index] = value;

  if (++self.ula_clip.index == 4) {
    self.ula_clip.index = 0;
    ula_clip_set(self.ula_clip.values[0], self.ula_clip.values[1], self.ula_clip.values[2], self.ula_clip.values[3]);
  }
}


static void nextreg_clip_window_tilemap_write(u8_t value) {
  self.tilemap_clip.values[self.tilemap_clip.index] = value;

  if (++self.tilemap_clip.index == 4) {
    self.tilemap_clip.index = 0;
    tilemap_clip_set(self.tilemap_clip.values[0], self.tilemap_clip.values[1], self.tilemap_clip.values[2], self.tilemap_clip.values[3]);
  }
}


static void nextreg_clip_window_sprites_write(u8_t value) {
  self.sprites_clip.values[self.sprites_clip.index] = value;

  if (++self.sprites_clip.index == 4) {
    self.sprites_clip.index = 0;
    sprites_clip_set(self.sprites_clip.values[0], self.sprites_clip.values[1], self.sprites_clip.values[2], self.sprites_clip.values[3]);
  }
}


static void nextreg_clip_window_layer2_write(u8_t value) {
  self.layer2_clip.values[self.layer2_clip.index] = value;

  if (++self.layer2_clip.index == 4) {
    self.layer2_clip.index = 0;
    layer2_clip_set(self.layer2_clip.values[0], self.layer2_clip.values[1], self.layer2_clip.values[2], self.layer2_clip.values[3]);
  }
}


static u8_t nextreg_clip_window_control_read(void) {
  return self.tilemap_clip.index << 6
       | self.ula_clip.index     << 4
       | self.sprites_clip.index << 2
       | self.layer2_clip.index;
}


static void nextreg_clip_window_control_write(u8_t value) {
  if (value & 0x08) self.tilemap_clip.index = 0;
  if (value & 0x04) self.ula_clip.index     = 0;
  if (value & 0x02) self.sprites_clip.index = 0;
  if (value & 0x01) self.layer2_clip.index  = 0;
}


static u8_t nextreg_palette_control_read(void) {
  return self.palette_disable_auto_increment << 7
       | self.palette_selected               << 4
       | self.is_palette_sprites_second      << 3
       | self.is_palette_layer2_second       << 2
       | self.is_palette_ula_second          << 1
       | self.ula_next_mode;
}


static void nextreg_palette_control_write(u8_t value) {
  self.palette_disable_auto_increment = value >> 7;
  self.palette_selected               = (value & 0x70) >> 4;
  self.is_palette_sprites_second      = (value & 0x08) >> 3;
  self.is_palette_layer2_second       = (value & 0x04) >> 2;
  self.is_palette_ula_second          = (value & 0x02) >> 1;
  self.ula_next_mode                  = value & 0x01;

  ula_next_mode_enable(self.ula_next_mode);
  ula_palette_set(self.is_palette_ula_second);
  layer2_palette_set(self.is_palette_layer2_second);
  sprites_palette_set(self.is_palette_sprites_second);

  self.palette_index_9bit_is_first_write = 1;
}


static void nextreg_palette_index_write(u8_t value) {
  self.palette_index                     = value;
  self.palette_index_9bit_is_first_write = 1;
}


static u8_t nextreg_palette_value_8bits_read(void) {
  return palette_read(self.palette_selected, self.palette_index)->rgb8;
}


static void nextreg_palette_value_8bits_write(u8_t value) {
  palette_write_rgb8(self.palette_selected, self.palette_index, value);
  self.palette_index_9bit_is_first_write = 1;
  if (!self.palette_disable_auto_increment) {
    self.palette_index++;
  }
}


static u8_t nextreg_palette_value_9bits_read(void) {
  const palette_entry_t* entry = palette_read(self.palette_selected, self.palette_index);
  return (entry->is_layer2_priority << 7) | (entry->rgb9 & 1);
}


static void nextreg_palette_value_9bits_write(u8_t value) {
  if (self.palette_index_9bit_is_first_write) {
    palette_write_rgb8(self.palette_selected, self.palette_index, value);
  } else {
    palette_write_rgb9(self.palette_selected, self.palette_index, value);
    if (!self.palette_disable_auto_increment) {
      self.palette_index++;
    }
  }

  self.palette_index_9bit_is_first_write ^= 1;  
}


static void nextreg_internal_port_decoding_0_write(u8_t value) {
  io_port_enable(0x7FFD, value & 0x02);
}


static void nextreg_internal_port_decoding_1_write(u8_t value) {
  io_mf_port_decoding_enable(value & 0x02);
}


static void nextreg_internal_port_decoding_2_write(u8_t value) {
  io_port_enable(0xBFFD, value & 0x01);
  io_port_enable(0xFFFD, value & 0x01);
}


static void nextreg_internal_port_decoding_3_write(u8_t value) {
}


static u8_t nextreg_sprite_layers_system_read(void) {
  return (slu_layer_priority_get() << 2)                      |
    (sprites_priority_get()                    ? 0x40 : 0x00) |
    (sprites_enable_clipping_over_border_get() ? 0x20 : 0x00) |
    (sprites_enable_over_border_get()          ? 0x02 : 0x00) |
    (sprites_enable_get()                      ? 0x01 : 0x00);
}


static void nextreg_sprite_layers_system_write(u8_t value) {
  ula_lo_res_enable_set(value & 0x80);
  slu_layer_priority_set((value & 0x1C) >> 2);
  sprites_priority_set(value & 0x40);
  sprites_enable_clipping_over_border_set(value & 0x20);
  sprites_enable_over_border_set(value & 0x02);
  sprites_enable_set(value & 0x01);
}


static void nextreg_interrupt_control_write(u8_t value) {
  /* TODO */
}


static void nextreg_int_en_0_write(u8_t value) {
  slu_line_interrupt_enable_set(value & 0x02);
  ula_irq_enable_set(value & 0x01);
}


void nextreg_data_write(u16_t address, u8_t value) {
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

    case E_NEXTREG_REGISTER_PERIPHERAL_5_SETTING:
      nextreg_peripheral_5_setting_write(value);
      break;

    case E_NEXTREG_REGISTER_CPU_SPEED:
      nextreg_cpu_speed_write(value);
      break;

    case E_NEXTREG_REGISTER_VIDEO_TIMING:
      if (config_is_active()) {
        clock_timing_write(value);
      }
      break;

    case E_NEXTREG_REGISTER_LAYER2_ACTIVE_RAM_BANK:
      layer2_active_bank_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_SHADOW_RAM_BANK:
      layer2_shadow_bank_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_CONTROL:
      layer2_control_write(value);
      break;

    case E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM:
      nextreg_sprite_layers_system_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_ULA:
      nextreg_clip_window_ula_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_TILEMAP:
      nextreg_clip_window_tilemap_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_SPRITES:
      nextreg_clip_window_sprites_write(value);
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_LAYER2:
      nextreg_clip_window_layer2_write(value);
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

    case E_NEXTREG_REGISTER_GLOBAL_TRANSPARENCY_COLOUR:
      slu_transparent_set(value);
      break;

    case E_NEXTREG_REGISTER_FALLBACK_COLOUR:
      slu_transparency_fallback_colour_write(value);
      break;

    case E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL:
      mmu_page_set(reg - E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL, value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_0:
      nextreg_internal_port_decoding_0_write(value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_1:
      nextreg_internal_port_decoding_1_write(value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_2:
      nextreg_internal_port_decoding_2_write(value);
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_3:
      nextreg_internal_port_decoding_3_write(value);
      break;

    case E_NEXTREG_REGISTER_ALTERNATE_ROM:
      nextreg_alternate_rom_write(value);
      break;

    case E_NEXTREG_REGISTER_SPECTRUM_MEMORY_MAPPING:
      nextreg_spectrum_memory_mapping_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_CONTROL:
      tilemap_tilemap_control_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_DEFAULT_TILEMAP_ATTRIBUTE:
      tilemap_default_tilemap_attribute_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_TILEMAP_BASE_ADDRESS:
      tilemap_tilemap_base_address_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_TILE_DEFINITIONS_BASE_ADDRESS:
      tilemap_tilemap_tile_definitions_address_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_TRANSPARENCY_INDEX:
      tilemap_transparency_index_write(value);
      break;

    case E_NEXTREG_REGISTER_SPRITES_TRANSPARENCY_INDEX:
      sprites_transparency_index_write(value);
      break;

    case E_NEXTREG_REGISTER_ULA_CONTROL:
      slu_ula_control_write(value);
      break;

    case E_NEXTREG_REGISTER_DISPLAY_CONTROL_1:
      layer2_enable(value >> 7);
      /* TODO: other bits in this register. */
      break;

    case E_NEXTREG_REGISTER_TILEMAP_X_SCROLL_MSB:
      tilemap_offset_x_msb_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_X_SCROLL_LSB:
      tilemap_offset_x_lsb_write(value);
      break;

    case E_NEXTREG_REGISTER_TILEMAP_Y_SCROLL:
      tilemap_offset_y_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_X_SCROLL_MSB:
      layer2_offset_x_msb_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_X_SCROLL_LSB:
      layer2_offset_x_lsb_write(value);
      break;

    case E_NEXTREG_REGISTER_LAYER2_Y_SCROLL:
      layer2_offset_y_write(value);
      break;

    case E_NEXTREG_REGISTER_LO_RES_X_SCROLL:
      ula_lo_res_offset_x_write(value);
      break;

    case E_NEXTREG_REGISTER_LO_RES_Y_SCROLL:
      ula_lo_res_offset_y_write(value);
      break;

    case E_NEXTREG_REGISTER_ULANEXT_ATTRIBUTE_BYTE_FORMAT:
      ula_attribute_byte_format_write(value);
      break;

    case E_NEXTREG_REGISTER_SPRITE_NUMBER:
      if (self.is_sprites_lockstepped) {
        sprites_slot_set(value);
      } else {
        self.sprite_number = value & 0x7F;
      }
      break;

    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_1:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_2:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_3:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_4:
    {
      const u8_t sprite_number = self.is_sprites_lockstepped ? sprites_slot_get() : self.sprite_number;
      sprites_attribute_set(sprite_number, reg - E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0, value);
    }
    break;

    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0_POST_INCREMENT:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_1_POST_INCREMENT:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_2_POST_INCREMENT:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_3_POST_INCREMENT:
    case E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_4_POST_INCREMENT:
    {
      const u8_t sprite_number = self.is_sprites_lockstepped ? sprites_slot_get() : self.sprite_number;
      sprites_attribute_set(sprite_number, reg - E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0_POST_INCREMENT, value);
      if (self.is_sprites_lockstepped) {
        sprites_slot_set(sprite_number + 1);
      } else {
        self.sprite_number = (self.sprite_number + 1) & 0x7F;
      }
    }
    break;

    case E_NEXTREG_REGISTER_COPPER_DATA_8BIT:
      copper_data_8bit_write(value);
      break;

    case E_NEXTREG_REGISTER_COPPER_ADDRESS:
      copper_address_write(value);
      break;

    case E_NEXTREG_REGISTER_COPPER_CONTROL:
      copper_control_write(value);
      break;

    case E_NEXTREG_REGISTER_COPPER_DATA_16BIT:
      copper_data_16bit_write(value);
      break;

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_CONTROL:
      slu_line_interrupt_control_write(value);
      break;

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_VALUE_LSB:
      slu_line_interrupt_value_lsb_write(value);
      break;

    case E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_MSB:
    case E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_LSB:
      /* Ignore these. */
      break;
      
    case E_NEXTREG_REGISTER_DAC_B_MIRROR:
      dac_mirror_write(DAC_B, value);
      break;

    case E_NEXTREG_REGISTER_DAC_A_D_MIRROR:
      dac_mirror_write(DAC_A | DAC_D, value);
      break;

     case E_NEXTREG_REGISTER_DAC_C_MIRROR:
      dac_mirror_write(DAC_C, value);
      break;

    case E_NEXTREG_REGISTER_INTERRUPT_CONTROL:
      nextreg_interrupt_control_write(value);
      break;

    case E_NEXTREG_REGISTER_INT_EN_0:
      nextreg_int_en_0_write(value);
      break;

    default:
      log_wrn("nextreg: unimplemented write of $%02X to register $%02X from PC=$%04X\n", value, reg, cpu_pc_get());
      break;
  }

  /* Always remember the last value written. */
  self.registers[reg] = value;
}


u8_t nextreg_data_read(u16_t address) {
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

    case E_NEXTREG_REGISTER_CORE_BOOT:
      return nextreg_core_boot_read();

    case E_NEXTREG_REGISTER_CPU_SPEED:
      return nextreg_cpu_speed_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_1_SETTING:
      return nextreg_peripheral_1_setting_read();
      
    case E_NEXTREG_REGISTER_PERIPHERAL_2_SETTING:
      return nextreg_peripheral_2_setting_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_3_SETTING:
      return nextreg_peripheral_3_setting_read();

    case E_NEXTREG_REGISTER_PERIPHERAL_4_SETTING:
      return nextreg_peripheral_4_setting_read();

    case E_NEXTREG_REGISTER_VIDEO_TIMING:
      return clock_timing_read();

    case E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM:
      return nextreg_sprite_layers_system_read();

    case E_NEXTREG_REGISTER_CLIP_WINDOW_ULA:
      return self.ula_clip.values[self.ula_clip.index];

    case E_NEXTREG_REGISTER_CLIP_WINDOW_TILEMAP:
      return self.tilemap_clip.values[self.tilemap_clip.index];

    case E_NEXTREG_REGISTER_CLIP_WINDOW_SPRITES:
      return self.sprites_clip.values[self.sprites_clip.index];

    case E_NEXTREG_REGISTER_CLIP_WINDOW_LAYER2:
      return self.layer2_clip.values[self.layer2_clip.index];

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

    case E_NEXTREG_REGISTER_GLOBAL_TRANSPARENCY_COLOUR:
      return slu_transparent_get()->rgb8;

    case E_NEXTREG_REGISTER_ULANEXT_ATTRIBUTE_BYTE_FORMAT:
      return ula_attribute_byte_format_read();

    case E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL:
      return mmu_page_get(self.selected_register - E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL);

    case E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_MSB:
      return (slu_active_video_line_get() >> 8) & 0x01;

    case E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_LSB:
      return slu_active_video_line_get() & 0xFF;

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_CONTROL:
      return slu_line_interrupt_control_read();

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_VALUE_LSB:
      return slu_line_interrupt_value_lsb_read();

    default:
      log_wrn("nextreg: unimplemented read from register $%02X\n", reg);
      break;
  }

  /* By default, return the last value written. */
  return self.registers[reg];
}


