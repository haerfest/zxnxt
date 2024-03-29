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
#include "divmmc.h"
#include "dma.h"
#include "i2c.h"
#include "io.h"
#include "joystick.h"
#include "keyboard.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "mf.h"
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

#define CORE_VERSION_MAJOR       3
#define CORE_VERSION_MINOR       1
#define CORE_VERSION_SUB_MINOR  10

#define BOARD_ID              0x02


#define DESCRIBE(reg) { reg, #reg }


static const struct {
  nextreg_register_t reg;
  const char*        description;
} next_register_description[] = {
  DESCRIBE(E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0_TIMING),
  DESCRIBE(E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0_VALID),
  DESCRIBE(E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0),
  DESCRIBE(E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_1),
  DESCRIBE(E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_ALTERNATE_ROM),
  DESCRIBE(E_NEXTREG_REGISTER_BOARD_ID),
  DESCRIBE(E_NEXTREG_REGISTER_CLIP_WINDOW_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_CLIP_WINDOW_LAYER2),
  DESCRIBE(E_NEXTREG_REGISTER_CLIP_WINDOW_SPRITES),
  DESCRIBE(E_NEXTREG_REGISTER_CLIP_WINDOW_TILEMAP),
  DESCRIBE(E_NEXTREG_REGISTER_CLIP_WINDOW_ULA),
  DESCRIBE(E_NEXTREG_REGISTER_CONFIG_MAPPING),
  DESCRIBE(E_NEXTREG_REGISTER_COPPER_ADDRESS),
  DESCRIBE(E_NEXTREG_REGISTER_COPPER_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_COPPER_DATA_16BIT),
  DESCRIBE(E_NEXTREG_REGISTER_COPPER_DATA_8BIT),
  DESCRIBE(E_NEXTREG_REGISTER_CORE_BOOT),
  DESCRIBE(E_NEXTREG_REGISTER_CORE_VERSION_SUB_MINOR),
  DESCRIBE(E_NEXTREG_REGISTER_CORE_VERSION),
  DESCRIBE(E_NEXTREG_REGISTER_CPU_SPEED),
  DESCRIBE(E_NEXTREG_REGISTER_DAC_A_D_MIRROR),
  DESCRIBE(E_NEXTREG_REGISTER_DAC_B_MIRROR),
  DESCRIBE(E_NEXTREG_REGISTER_DAC_C_MIRROR),
  DESCRIBE(E_NEXTREG_REGISTER_DISPLAY_CONTROL_1),
  DESCRIBE(E_NEXTREG_REGISTER_DMA_INT_EN_0),
  DESCRIBE(E_NEXTREG_REGISTER_DMA_INT_EN_1),
  DESCRIBE(E_NEXTREG_REGISTER_DMA_INT_EN_2),
  DESCRIBE(E_NEXTREG_REGISTER_ESP_WIFI_GPIO_OUTPUT_ENABLE),
  DESCRIBE(E_NEXTREG_REGISTER_EXPANSION_BUS_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_EXPANSION_BUS_ENABLE),
  DESCRIBE(E_NEXTREG_REGISTER_EXPANSION_BUS_IO_PROPAGATE),
  DESCRIBE(E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_0),
  DESCRIBE(E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_1),
  DESCRIBE(E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_2),
  DESCRIBE(E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_3),
  DESCRIBE(E_NEXTREG_REGISTER_FALLBACK_COLOUR),
  DESCRIBE(E_NEXTREG_REGISTER_GLOBAL_TRANSPARENCY_COLOUR),
  DESCRIBE(E_NEXTREG_REGISTER_INT_EN_0),
  DESCRIBE(E_NEXTREG_REGISTER_INT_EN_1),
  DESCRIBE(E_NEXTREG_REGISTER_INT_EN_2),
  DESCRIBE(E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_0),
  DESCRIBE(E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_1),
  DESCRIBE(E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_2),
  DESCRIBE(E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_3),
  DESCRIBE(E_NEXTREG_REGISTER_INTERRUPT_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_IO_TRAP_CAUSE),
  DESCRIBE(E_NEXTREG_REGISTER_IO_TRAP_WRITE),
  DESCRIBE(E_NEXTREG_REGISTER_IO_TRAPS),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_ACTIVE_RAM_BANK),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_SHADOW_RAM_BANK),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_X_SCROLL_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_X_SCROLL_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_LAYER2_Y_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_LINE_INTERRUPT_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_LINE_INTERRUPT_VALUE_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_LO_RES_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_LO_RES_X_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_LO_RES_Y_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_MACHINE_ID),
  DESCRIBE(E_NEXTREG_REGISTER_MACHINE_TYPE),
  DESCRIBE(E_NEXTREG_REGISTER_MEMORY_MAPPING_MODE),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_PALETTE_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_PALETTE_INDEX),
  DESCRIBE(E_NEXTREG_REGISTER_PALETTE_VALUE_8BITS),
  DESCRIBE(E_NEXTREG_REGISTER_PALETTE_VALUE_9BITS),
  DESCRIBE(E_NEXTREG_REGISTER_PERIPHERAL_1_SETTING),
  DESCRIBE(E_NEXTREG_REGISTER_PERIPHERAL_2_SETTING),
  DESCRIBE(E_NEXTREG_REGISTER_PERIPHERAL_3_SETTING),
  DESCRIBE(E_NEXTREG_REGISTER_PERIPHERAL_4_SETTING),
  DESCRIBE(E_NEXTREG_REGISTER_PERIPHERAL_5_SETTING),
  DESCRIBE(E_NEXTREG_REGISTER_PI_GPIO_OUTPUT_ENABLE_0),
  DESCRIBE(E_NEXTREG_REGISTER_PI_GPIO_OUTPUT_ENABLE_1),
  DESCRIBE(E_NEXTREG_REGISTER_PI_GPIO_OUTPUT_ENABLE_2),
  DESCRIBE(E_NEXTREG_REGISTER_PI_GPIO_OUTPUT_ENABLE_3),
  DESCRIBE(E_NEXTREG_REGISTER_PI_I2S_AUDIO_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_PI_PERIPHERAL_ENABLE),
  DESCRIBE(E_NEXTREG_REGISTER_PS2_KEYMAP_ADDRESS_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_PS2_KEYMAP_ADDRESS_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_RESET),
  DESCRIBE(E_NEXTREG_REGISTER_SPECTRUM_MEMORY_MAPPING),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0_POST_INCREMENT),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_0),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_1_POST_INCREMENT),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_1),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_2_POST_INCREMENT),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_2),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_3_POST_INCREMENT),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_3),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_4_POST_INCREMENT),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_ATTRIBUTE_4),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITE_NUMBER),
  DESCRIBE(E_NEXTREG_REGISTER_SPRITES_TRANSPARENCY_INDEX),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_DEFAULT_TILEMAP_ATTRIBUTE),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_TILE_DEFINITIONS_BASE_ADDRESS),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_TILEMAP_BASE_ADDRESS),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_TRANSPARENCY_INDEX),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_X_SCROLL_LSB),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_X_SCROLL_MSB),
  DESCRIBE(E_NEXTREG_REGISTER_TILEMAP_Y_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_ULA_CONTROL),
  DESCRIBE(E_NEXTREG_REGISTER_ULA_X_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_ULA_Y_SCROLL),
  DESCRIBE(E_NEXTREG_REGISTER_ULANEXT_ATTRIBUTE_BYTE_FORMAT),
  DESCRIBE(E_NEXTREG_REGISTER_USER_0),
  DESCRIBE(E_NEXTREG_REGISTER_VERTICAL_LINE_COUNT_OFFSET),
  DESCRIBE(E_NEXTREG_REGISTER_VIDEO_TIMING),
};


typedef struct clip_t {
  u8_t index;
  u8_t values[4];  /* x1, x2, y1, y2. */
} clip_t;


typedef struct nextreg_t {
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
  int                      is_hotkey_cpu_speed_enabled;
  int                      is_hotkey_nmi_multiface_enabled;
  int                      is_hotkey_nmi_divmmc_enabled;
  u8_t                     sprite_number;
  int                      is_sprites_lockstepped;
  u8_t                     nmi_return_address_lsb;
  u8_t                     nmi_return_address_msb;
  u8_t                     user_register_0;
} nextreg_t;


static nextreg_t self;


static void nextreg_cpu_speed_write(u8_t value);


static const char* nextreg_description(u8_t reg) {
  for (size_t i = 0; i < sizeof(next_register_description) / sizeof(*next_register_description); i++) {
    if (next_register_description[i].reg == reg) {
      return next_register_description[i].description;
    }
  }

  return "?";
}


void nextreg_reset(reset_t reset) {
  log_wrn("nextreg: %s reset\n", reset == E_RESET_SOFT ? "soft" : "hard");

  if (reset == E_RESET_HARD) {
    memset(&self, 0, sizeof(self));

    self.is_hard_reset                   = 1;
    self.altrom_soft_reset_during_writes = 1;
    self.is_hotkey_nmi_multiface_enabled = 0;
    self.is_hotkey_nmi_divmmc_enabled    = 0;

    bootrom_activate();
    config_set_rom_ram_bank(0);
    config_activate();
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
  self.nmi_return_address_lsb            = 0x00;
  self.nmi_return_address_lsb            = 0x00;
  self.user_register_0                   = 0xFF;

  cpu_reset(reset);
  ay_reset(reset);
  copper_reset(reset);
  dac_reset(reset);
  divmmc_reset(reset);
  dma_reset(reset);
  i2c_reset(reset);
  io_reset(reset);
  layer2_reset(reset);
  mf_reset(reset);
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

  nextreg_write_internal(E_NEXTREG_REGISTER_CPU_SPEED, E_CPU_SPEED_3MHZ);
  nextreg_write_internal(E_NEXTREG_REGISTER_TILEMAP_CONTROL, 0x00);
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


static u8_t nextreg_reset_read(void) {
  u8_t result = self.is_hard_reset ? 0x02 : 0x01;
  int  nmi_active = cpu_nmi_active();

  if (nmi_active & CPU_NMI_MF_VIA_IO_TRAP) {
    result |= 0x10;
  }
  if (nmi_active & CPU_NMI_MF_VIA_NEXTREG) {
    result |= 0x08;
  }
  if (nmi_active & CPU_NMI_DIVMMC_VIA_NEXTREG) {
    result |= 0x04;
  }

  return result;
}


static void nextreg_reset_write(u8_t value) {
  log_wrn("nextreg: write reset state $%02X\n", value);

  if (value & 0x03) {
    /* Hard or soft reset. */
    self.is_hard_reset = value & 0x02;
    nextreg_reset(self.is_hard_reset ? E_RESET_HARD : E_RESET_SOFT);
    return;
  }

  if (!mf_is_active() && !divmmc_is_active()) {
    if ((value & 0x10) == 0x00) {
      io_trap_clear();
    }

    if (value & 0x08) {
      cpu_nmi(CPU_NMI_MF_VIA_NEXTREG);
    } else {
      cpu_nmi_clear(CPU_NMI_MF_VIA_NEXTREG);
    }

    if (value & 0x04) {
      cpu_nmi(CPU_NMI_DIVMMC_VIA_NEXTREG);
    } else {
      cpu_nmi_clear(CPU_NMI_DIVMMC_VIA_NEXTREG);
    }
  }
}


static void nextreg_config_mapping_write(u8_t value) {
  /* Only bits 4:0 are specified, but FPGA uses bits 6:0. */
  config_set_rom_ram_bank(value & 0x7F);
}


static u8_t nextreg_machine_type_read(void) {
  u8_t result = self.palette_index_9bit_is_first_write ? 0x00 : 0x80;

  switch (ula_timing_get()) {
    case E_MACHINE_TYPE_CONFIG_MODE:
      break;
    case E_MACHINE_TYPE_ZX_48K:
      result |= 0x01 << 4;
      break;
    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      result |= 0x02 << 4;
      break;
    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      result |= 0x03 << 4;
      break;
    case E_MACHINE_TYPE_PENTAGON:
      result |= 0x04 << 4;
      break;
  }

  switch (rom_machine_type_get()) {
    case E_MACHINE_TYPE_CONFIG_MODE:
      break;
    case E_MACHINE_TYPE_ZX_48K:
      result |= 0x01;
      break;
    case E_MACHINE_TYPE_ZX_128K_PLUS2:
      result |= 0x02;
      break;
    case E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      result |= 0x03;
      break;
    case E_MACHINE_TYPE_PENTAGON:
      result |= 0x04;
      break;
  }

  /* TODO Return display timing lock. */

  return result;
}


static void nextreg_machine_type_write(u8_t value) {
  if (value & 0x80) {
    const u8_t machine_type = (value >> 4) & 0x03;
    if (machine_type <= E_MACHINE_TYPE_PENTAGON) {
      ula_timing_set(machine_type);
    }
  }

  /* TODO Handle display timing lock. */

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

    rom_machine_type_set(machine_type);
  }
}


static u8_t nextreg_core_boot_read(void) {
  return keyboard_is_special_key_pressed(E_KEYBOARD_SPECIAL_KEY_DRIVE) << 1
       | keyboard_is_special_key_pressed(E_KEYBOARD_SPECIAL_KEY_NMI);
}


static u8_t nextreg_peripheral_1_setting_read(void) {
  const u8_t j1 = joystick_type_get(E_JOYSTICK_LEFT);
  const u8_t j2 = joystick_type_get(E_JOYSTICK_RIGHT);

  return ((j1 & 0x03) << 6)
       | ((j1 & 0x04) << 1)
       | ((j2 & 0x03) << 4)
       | ((j2 & 0x04) >> 1)
       | (ula_60hz_get() ? 0x04 : 0x00);
}


static void nextreg_peripheral_1_setting_write(u8_t value) {
  joystick_type_set(E_JOYSTICK_LEFT, (((value & 0x08) >> 1) | (value & 0xC0) >> 6));
  joystick_type_set(E_JOYSTICK_RIGHT, (((value & 0x02) << 1) | (value & 0x30) >> 4));
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
       | (ay_stereo_acb_get() != 0) << 5
       | (ay_turbosound_enable_get() != 0) << 1;
}


static void nextreg_peripheral_3_setting_write(u8_t value) {
  if (value & 0x80) {
    paging_spectrum_128k_paging_unlock();
  }

  ula_contention_set((value & 0x40) == 0);
  ay_stereo_acb_set(value & 0x20);
  dac_enable((value & 0x08) >> 3);
  ula_timex_video_mode_read_enable((value & 0x04) != 0);
  ay_turbosound_enable_set(value & 0x02);
}


static u8_t nextreg_peripheral_4_setting_read(void) {
  return (ay_mono_enable_get(2) != 0) << 7
       | (ay_mono_enable_get(1) != 0) << 6
       | (ay_mono_enable_get(0) != 0) << 5
       | self.is_sprites_lockstepped  << 4;
}


static void nextreg_peripheral_4_setting_write(u8_t value) {
  ay_mono_enable_set(2, value & 0x80);
  ay_mono_enable_set(1, value & 0x40);
  ay_mono_enable_set(0, value & 0x20);

  if (value & 0x08) {
    divmmc_mapram_disable();
  }

  self.is_sprites_lockstepped = (value & 0x10) >> 4;
}


static u8_t nextreg_peripheral_5_setting_read(void) {
  /* TODO Return other bits. */
  return (mf_type_get() << 5) | (divmmc_is_automap_enabled() ? 0x10 : 0x00);
}


static void nextreg_peripheral_5_setting_write(u8_t value) {
  if (config_is_active()) {
    mf_type_set((value & 0xC0) >> 5);
  }

  divmmc_automap_enable(value & 0x10);
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


static u8_t nextreg_sprite_layers_system_read(void) {
  return \
    (ula_lo_res_enable_get()                   ? 0x80 : 0x00) |
    (slu_layer_priority_get() << 2)                           |
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


static u8_t nextreg_interrupt_control_read(void) {
  /* TODO Implement other bits of this register. */
  return cpu_stackless_nmi_enabled() ? 0x08 : 0x00;
}


static void nextreg_interrupt_control_write(u8_t value) {
  cpu_stackless_nmi_enable(value & 0x08);
  /* TODO Implement other bits of this register. */
}


static u8_t nextreg_int_en_0_read(void) {
  /* TODO Implement other bits of this register. */
  return ula_irq_enable_get() ? 0x01 : 0x00;
}


static void nextreg_int_en_0_write(u8_t value) {
  slu_line_interrupt_enable_set(value & 0x02);
  ula_irq_enable_set(value & 0x01);
}


 void nextreg_data_write(u16_t address, u8_t value) {
  if (!nextreg_write_internal(self.selected_register, value)) {
    log_wrn("nextreg: unimplemented write of $%02X to register $%02X (%s) from PC=$%04X\n", value, self.selected_register, nextreg_description(self.selected_register), cpu_pc_get());
  }
}


int nextreg_write_internal(u8_t reg, u8_t value) {
  /* Always remember the last value written. */
  self.registers[reg] = value;

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
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_1:
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_2:
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_3:
      io_decoding_write(reg - E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_0, value);
      break;

    case E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_0:
    case E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_1:
    case E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_2:
    case E_NEXTREG_REGISTER_EXTERNAL_PORT_DECODING_3:
      /* Ignored. */
      break;

    case E_NEXTREG_REGISTER_EXPANSION_BUS_ENABLE:
    case E_NEXTREG_REGISTER_EXPANSION_BUS_CONTROL:
      /* Ignored. */
      break;

    case E_NEXTREG_REGISTER_USER_0:
      self.user_register_0 = value;
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

    case E_NEXTREG_REGISTER_ULA_X_SCROLL:
      ula_offset_x_write(value);
      break;

    case E_NEXTREG_REGISTER_ULA_Y_SCROLL:
      ula_offset_y_write(value);
      break;

    case E_NEXTREG_REGISTER_PS2_KEYMAP_ADDRESS_MSB:
    case E_NEXTREG_REGISTER_PS2_KEYMAP_ADDRESS_LSB:
    case E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_MSB:
    case E_NEXTREG_REGISTER_PS2_KEYMAP_DATA_LSB:
      /* Ignored. */
      break;
      
    case E_NEXTREG_REGISTER_DAC_B_MIRROR:
      dac_write(DAC_B, value);
      break;

    case E_NEXTREG_REGISTER_DAC_A_D_MIRROR:
      dac_write(DAC_A | DAC_D, value);
      break;

     case E_NEXTREG_REGISTER_DAC_C_MIRROR:
      dac_write(DAC_C, value);
      break;

    case E_NEXTREG_REGISTER_INTERRUPT_CONTROL:
      nextreg_interrupt_control_write(value);
      break;

    case E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_LSB:
      self.nmi_return_address_lsb = value;
      break;

    case E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_MSB:
      self.nmi_return_address_msb = value;
      break;

    case E_NEXTREG_REGISTER_INT_EN_0:
      nextreg_int_en_0_write(value);
      break;

    case E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0:
      divmmc_automap_on_fetch_enable(0x0038, value & 0x80);
      divmmc_automap_on_fetch_enable(0x0030, value & 0x40);
      divmmc_automap_on_fetch_enable(0x0028, value & 0x20);
      divmmc_automap_on_fetch_enable(0x0020, value & 0x10);
      divmmc_automap_on_fetch_enable(0x0018, value & 0x08);
      divmmc_automap_on_fetch_enable(0x0010, value & 0x04);
      divmmc_automap_on_fetch_enable(0x0008, value & 0x02);
      divmmc_automap_on_fetch_enable(0x0000, value & 0x01);
      break;

    case E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0_VALID:
      divmmc_automap_on_fetch_always(0x0038, value & 0x80);
      divmmc_automap_on_fetch_always(0x0030, value & 0x40);
      divmmc_automap_on_fetch_always(0x0028, value & 0x20);
      divmmc_automap_on_fetch_always(0x0020, value & 0x10);
      divmmc_automap_on_fetch_always(0x0018, value & 0x08);
      divmmc_automap_on_fetch_always(0x0010, value & 0x04);
      divmmc_automap_on_fetch_always(0x0008, value & 0x02);
      divmmc_automap_on_fetch_always(0x0000, value & 0x01);
      break;

    case E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_0_TIMING:
      divmmc_automap_on_fetch_instant(0x0038, value & 0x80);
      divmmc_automap_on_fetch_instant(0x0030, value & 0x40);
      divmmc_automap_on_fetch_instant(0x0028, value & 0x20);
      divmmc_automap_on_fetch_instant(0x0020, value & 0x10);
      divmmc_automap_on_fetch_instant(0x0018, value & 0x08);
      divmmc_automap_on_fetch_instant(0x0010, value & 0x04);
      divmmc_automap_on_fetch_instant(0x0008, value & 0x02);
      divmmc_automap_on_fetch_instant(0x0000, value & 0x01);
      break;

    case E_NEXTREG_REGISTER_DIVMMC_ENTRY_POINTS_1:
      divmmc_automap_on_fetch_enable(0x3D00, value & 0x80);
      divmmc_automap_on_fetch_enable(0x1FF8, value & 0x40);
      divmmc_automap_on_fetch_enable(0x056A, value & 0x20);
      divmmc_automap_on_fetch_enable(0x04D7, value & 0x10);
      divmmc_automap_on_fetch_enable(0x0562, value & 0x08);
      divmmc_automap_on_fetch_enable(0x04C6, value & 0x04);
      divmmc_automap_on_fetch_enable(0x0066, value & 0x03);
      break;

    case E_NEXTREG_REGISTER_IO_TRAPS:
      io_traps_enable(value & 0x01);
      break;

    default:
      return 0;
  }

  return 1;
}


u8_t nextreg_data_read(u16_t address) {
  u8_t value = 0;
  
  if (!nextreg_read_internal(self.selected_register, &value)) {
    log_wrn("nextreg: unimplemented read from register $%02X (%s)\n", self.selected_register, nextreg_description(self.selected_register));
  }
  return value;
}


int nextreg_read_internal(u8_t reg, u8_t* value) {
  /* By default, return the last value written. */
  *value = self.registers[reg];

  switch (reg) {
    case E_NEXTREG_REGISTER_MACHINE_ID:
      *value = MACHINE_ID;
      break;

    case E_NEXTREG_REGISTER_RESET:
      *value = nextreg_reset_read();
      break;

    case E_NEXTREG_REGISTER_MACHINE_TYPE:
      *value = nextreg_machine_type_read();
      break;

    case E_NEXTREG_REGISTER_CORE_VERSION:
      *value = CORE_VERSION_MAJOR << 4 | CORE_VERSION_MINOR;
      break;

    case E_NEXTREG_REGISTER_CORE_VERSION_SUB_MINOR:
      *value = CORE_VERSION_SUB_MINOR;
      break;

    case E_NEXTREG_REGISTER_BOARD_ID:
      *value = BOARD_ID;
      break;

    case E_NEXTREG_REGISTER_CORE_BOOT:
      *value = nextreg_core_boot_read();
      break;

    case E_NEXTREG_REGISTER_CPU_SPEED:
      *value = nextreg_cpu_speed_read();
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_1_SETTING:
      *value = nextreg_peripheral_1_setting_read();
      break;
      
    case E_NEXTREG_REGISTER_PERIPHERAL_2_SETTING:
      *value = nextreg_peripheral_2_setting_read();
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_3_SETTING:
      *value = nextreg_peripheral_3_setting_read();
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_4_SETTING:
      *value = nextreg_peripheral_4_setting_read();
      break;

    case E_NEXTREG_REGISTER_PERIPHERAL_5_SETTING:
      *value = nextreg_peripheral_5_setting_read();
      break;

    case E_NEXTREG_REGISTER_VIDEO_TIMING:
      *value = clock_timing_read();
      break;

    case E_NEXTREG_REGISTER_LAYER2_ACTIVE_RAM_BANK:
      *value = layer2_active_bank_read();
      break;

    case E_NEXTREG_REGISTER_LAYER2_SHADOW_RAM_BANK:
      *value = layer2_shadow_bank_read();
      break;

    case E_NEXTREG_REGISTER_SPRITE_LAYERS_SYSTEM:
      *value = nextreg_sprite_layers_system_read();
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_ULA:
      *value = self.ula_clip.values[self.ula_clip.index];
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_TILEMAP:
      *value = self.tilemap_clip.values[self.tilemap_clip.index];
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_SPRITES:
      *value = self.sprites_clip.values[self.sprites_clip.index];
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_LAYER2:
      *value = self.layer2_clip.values[self.layer2_clip.index];
      break;

    case E_NEXTREG_REGISTER_CLIP_WINDOW_CONTROL:
      *value = nextreg_clip_window_control_read();
      break;

    case E_NEXTREG_REGISTER_PALETTE_INDEX:
      *value = self.palette_index;
      break;

    case E_NEXTREG_REGISTER_PALETTE_CONTROL:
      *value = nextreg_palette_control_read();
      break;

    case E_NEXTREG_REGISTER_PALETTE_VALUE_8BITS:
      *value = nextreg_palette_value_8bits_read();
      break;

    case E_NEXTREG_REGISTER_PALETTE_VALUE_9BITS:
      *value = nextreg_palette_value_9bits_read();
      break;

    case E_NEXTREG_REGISTER_GLOBAL_TRANSPARENCY_COLOUR:
      *value = slu_transparent_get()->rgb8;
      break;

    case E_NEXTREG_REGISTER_ULANEXT_ATTRIBUTE_BYTE_FORMAT:
      *value = ula_attribute_byte_format_read();
      break;

    case E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT1_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT2_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT3_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT4_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT5_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT6_CONTROL:
    case E_NEXTREG_REGISTER_MMU_SLOT7_CONTROL:
      *value = mmu_page_get(self.selected_register - E_NEXTREG_REGISTER_MMU_SLOT0_CONTROL);
      break;

    case E_NEXTREG_REGISTER_USER_0:
      *value = self.user_register_0;
      break;
  
    case E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_MSB:
      *value = (slu_active_video_line_get() >> 8) & 0x01;
      break;

    case E_NEXTREG_REGISTER_ACTIVE_VIDEO_LINE_LSB:
      *value = slu_active_video_line_get() & 0xFF;
      break;

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_CONTROL:
      *value = slu_line_interrupt_control_read();
      break;

    case E_NEXTREG_REGISTER_LINE_INTERRUPT_VALUE_LSB:
      *value = slu_line_interrupt_value_lsb_read();
      break;

    case E_NEXTREG_REGISTER_ULA_X_SCROLL:
      *value = ula_offset_x_read();
      break;

    case E_NEXTREG_REGISTER_ULA_Y_SCROLL:
      *value = ula_offset_y_read();
      break;

    case E_NEXTREG_REGISTER_IO_TRAPS:
      *value = io_are_traps_enabled() ? 0x01 : 0x00;
      break;

    case E_NEXTREG_REGISTER_IO_TRAP_WRITE:
      *value = io_trap_byte_written();
      break;

    case E_NEXTREG_REGISTER_IO_TRAP_CAUSE:
      *value = (u8_t) io_trap_cause();
      break;

    case E_NEXTREG_REGISTER_INTERRUPT_CONTROL:
      *value = nextreg_interrupt_control_read();
      break;

    case E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_LSB:
      *value = self.nmi_return_address_lsb;
      break;

    case E_NEXTREG_REGISTER_NMI_RETURN_ADDRESS_MSB:
      *value = self.nmi_return_address_msb;
      break;

    case E_NEXTREG_REGISTER_INT_EN_0:
      *value = nextreg_int_en_0_read();
      break;

    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_0:
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_1:
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_2:
    case E_NEXTREG_REGISTER_INTERNAL_PORT_DECODING_3:
    case E_NEXTREG_REGISTER_TILEMAP_CONTROL:
    case E_NEXTREG_REGISTER_FALLBACK_COLOUR:
      /* Cached value is ok, don't issue a warning. */
      break;

    default:
      /* Might be an unimplemented read, issue a warning. */
      return 0;
  }

  return 1;
}


