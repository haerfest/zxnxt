#include <string.h>
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "rom.h"
#include "utils.h"


/**
 * DivMMC seems to be a backwards-compatible evolution of DivIDE.
 * See: https://velesoft.speccy.cz/zx/divide/divide-memory.htm
 *
 * For specifics to the Next, see:
 * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/commit/0a9e835a42975600dedf6653e078022281c1dd46
 */


 typedef enum {
  E_DIVMMC_ADDR_FIRST = 0,
  E_DIVMMC_ADDR_0000 = E_DIVMMC_ADDR_FIRST,
  E_DIVMMC_ADDR_0008,
  E_DIVMMC_ADDR_0010,
  E_DIVMMC_ADDR_0018,
  E_DIVMMC_ADDR_0020,
  E_DIVMMC_ADDR_0028,
  E_DIVMMC_ADDR_0030,
  E_DIVMMC_ADDR_0038,
  E_DIVMMC_ADDR_0066,
  E_DIVMMC_ADDR_04C6,
  E_DIVMMC_ADDR_04D7,
  E_DIVMMC_ADDR_0562,
  E_DIVMMC_ADDR_056A,
  E_DIVMMC_ADDR_1FF8_1FFF,
  E_DIVMMC_ADDR_3DXX,
  E_DIVMMC_ADDR_LAST = E_DIVMMC_ADDR_3DXX,
  E_DIVMMC_ADDR_NONE
} divmmc_addr_t;


typedef struct divmmc_automap_t {
  int enable;
  int always;
  int instant;
} divmmc_automap_t;


typedef struct divmmc_t {
  u8_t*            sram;
  u8_t*            rom;
  u8_t*            ram;
  u8_t             value;
  int              bank_number;
  int              is_active_via_conmem;
  int              is_active_via_automap;
  int              is_mapram_enabled;
  int              is_automap_enabled;
  divmmc_automap_t automap[E_DIVMMC_ADDR_LAST - E_DIVMMC_ADDR_FIRST + 1];
} divmmc_t;


static divmmc_t self;


static void divmmc_refresh_ptrs(void) {
  /* We subtract 0x2000 because RAM is paged in starting at 0x2000, so all
   * addresses are offset 0x2000. This saves us a subtraction on every
   * access. */
  self.ram = &self.sram[MEMORY_RAM_OFFSET_DIVMMC_RAM + self.bank_number * 8 * 1024 - 0x2000];

  if (self.is_mapram_enabled) {
    self.rom = &self.sram[MEMORY_RAM_OFFSET_DIVMMC_RAM + 3 * 8 * 1024];
  } else {
    self.rom = &self.sram[MEMORY_RAM_OFFSET_DIVMMC_ROM];
  }
}


int divmmc_init(u8_t* sram) {
  memset(&self, 0, sizeof(self));
  self.sram = sram;

  divmmc_reset(E_RESET_HARD);
  divmmc_refresh_ptrs();

  return 0;
}


void divmmc_finit(void) {
}


void divmmc_reset(reset_t reset) {
  memset(self.automap, 0, sizeof(self.automap));

  self.automap[E_DIVMMC_ADDR_0000     ].enable = 1;
  self.automap[E_DIVMMC_ADDR_0000     ].always = 1;
  self.automap[E_DIVMMC_ADDR_0008     ].enable = 1;
  self.automap[E_DIVMMC_ADDR_0038     ].enable = 1;
  self.automap[E_DIVMMC_ADDR_0066     ].enable = 1;
  self.automap[E_DIVMMC_ADDR_04C6     ].enable = 1;
  self.automap[E_DIVMMC_ADDR_0562     ].enable = 1; 
  self.automap[E_DIVMMC_ADDR_1FF8_1FFF].enable = 1;
  self.automap[E_DIVMMC_ADDR_3DXX     ].enable = 1;

  if (reset == E_RESET_HARD) {
    self.is_automap_enabled    = 0;
    self.value                 = 0;
    self.is_mapram_enabled     = 0;
    self.is_active_via_conmem  = 0;
    self.is_active_via_automap = 0;

    divmmc_refresh_ptrs();
    memory_refresh_accessors(0, 2);
  }
}


int divmmc_is_active(void) {
  return self.is_active_via_conmem || self.is_active_via_automap;
}


u8_t divmmc_rom_read(u16_t address) {
  return self.rom[address];
}


void divmmc_rom_write(u16_t address, u8_t value) {
}


u8_t divmmc_ram_read(u16_t address) {
  return self.ram[address];
}


void divmmc_ram_write(u16_t address, u8_t value) {
  self.ram[address] = value;
}


u8_t divmmc_control_read(u16_t address) {
  return self.value;
}


void divmmc_control_write(u16_t address, u8_t value) {
  //log_wrn("divmmc: control write $%02X (PC=$%04X)\n", value, cpu_pc_get());

  self.value                = value | self.is_mapram_enabled;
  self.is_mapram_enabled   |= value & 0x40;
  self.bank_number          = value & 0x0F;
  self.is_active_via_conmem = value & 0x80;

  divmmc_refresh_ptrs();
  memory_refresh_accessors(0, 2);
}


int divmmc_is_automap_enabled(void) {
  return self.is_automap_enabled;
}


void divmmc_automap_enable(int enable) {
  log_wrn("divmmc: automap set to %s\n", enable ? "enabled" : "disabled");

  self.is_automap_enabled = enable;
}


static divmmc_addr_t map(u16_t address) {
  switch (address) {
    case 0x0000: return E_DIVMMC_ADDR_0000; break;
    case 0x0008: return E_DIVMMC_ADDR_0008; break;
    case 0x0010: return E_DIVMMC_ADDR_0010; break;
    case 0x0018: return E_DIVMMC_ADDR_0018; break;
    case 0x0020: return E_DIVMMC_ADDR_0020; break;
    case 0x0028: return E_DIVMMC_ADDR_0028; break;
    case 0x0030: return E_DIVMMC_ADDR_0030; break;
    case 0x0038: return E_DIVMMC_ADDR_0038; break;
    case 0x0066: return E_DIVMMC_ADDR_0066; break;
    case 0x04C6: return E_DIVMMC_ADDR_04C6; break;
    case 0x04D7: return E_DIVMMC_ADDR_04D7; break;
    case 0x0562: return E_DIVMMC_ADDR_0562; break;
    case 0x056A: return E_DIVMMC_ADDR_056A; break;
    
    case 0x1FF8:
    case 0x1FF9:
    case 0x1FFA:
    case 0x1FFB:
    case 0x1FFC:
    case 0x1FFD:
    case 0x1FFE:
    case 0x1FFF:
      return E_DIVMMC_ADDR_1FF8_1FFF;

    default:
      return (address >> 8 == 0x3D) ? E_DIVMMC_ADDR_3DXX : E_DIVMMC_ADDR_NONE;
  }
}


void divmmc_automap_on_fetch_enable(u16_t address, int enable) {
  const u16_t addr = map(address);
  if (addr != E_DIVMMC_ADDR_NONE) {
    self.automap[addr].enable = enable;
    log_wrn("divmmc: automap on fetch $%04X %s\n", address, enable ? "enable" : "disable");
  }
}


void divmmc_automap_on_fetch_always(u16_t address, int always) {
  const u16_t addr = map(address);
  if (addr >= E_DIVMMC_ADDR_0000 && addr <= E_DIVMMC_ADDR_0038) {
    self.automap[addr].always = always;
    log_wrn("divmmc: automap on fetch $%04X %s\n", address, always ? "always" : "rom3");
  }
}


void divmmc_automap_on_fetch_instant(u16_t address, int instant) {
  const u16_t addr = map(address);
  if (addr >= E_DIVMMC_ADDR_0000 && addr <= E_DIVMMC_ADDR_0038) {
    self.automap[addr].instant = instant;
    log_wrn("divmmc: automap on fetch $%04X %s\n", address, instant ? "instant" : "delayed");
  }
}


void divmmc_automap(u16_t address, int instant) {
  if (!self.is_automap_enabled) {
    return;
  }

  const divmmc_addr_t addr = map(address);
  if (!self.automap[addr].enable) {
    return;
  }
  
  /* Address-specific checks. */
  switch (addr) {
    case E_DIVMMC_ADDR_NONE:
      return;

    case E_DIVMMC_ADDR_0000:
    case E_DIVMMC_ADDR_0008:
    case E_DIVMMC_ADDR_0010:
    case E_DIVMMC_ADDR_0018:
    case E_DIVMMC_ADDR_0020:
    case E_DIVMMC_ADDR_0028:
    case E_DIVMMC_ADDR_0030:
    case E_DIVMMC_ADDR_0038:
      if (self.automap[addr].instant != instant) {
        return;
      }
      if (!self.automap[addr].always && rom_selected() != E_ROM_48K_BASIC) {
        return;
      }
      break;

    case E_DIVMMC_ADDR_0066:
      if (self.automap[addr].instant != instant || !keyboard_is_special_key_pressed(E_KEYBOARD_SPECIAL_KEY_NMI)) {
        return;
      }
      break;

    case E_DIVMMC_ADDR_04C6:
    case E_DIVMMC_ADDR_04D7:
    case E_DIVMMC_ADDR_0562:
    case E_DIVMMC_ADDR_056A:
      if (instant || rom_selected() != E_ROM_48K_BASIC) {
        return;
      }
      break;

    case E_DIVMMC_ADDR_1FF8_1FFF:
      if (instant) {
        return;
      }
      break;

    case E_DIVMMC_ADDR_3DXX:
      if (!instant || rom_selected() != E_ROM_48K_BASIC) {
        return;
      }
      break;
  }

  /* Address 1FF8-1FFF switches automap off, the rest switches it on. */
  self.is_active_via_automap = addr != E_DIVMMC_ADDR_1FF8_1FFF;
  log_wrn("divmmc: %s via automap on $%04X (%s)\n", self.is_active_via_automap ? "activated" : "deactivated", address, instant ? "instant" : "delayed");

  if (self.is_active_via_automap) {
    divmmc_refresh_ptrs();
  }
  memory_refresh_accessors(0, 2);
}


int divmmc_is_mapram_enabled(void) {
  return self.is_mapram_enabled;
}


void divmmc_mapram_disable(void) {
  log_wrn("divmmc: MAPRAM disabled\n");

  self.is_mapram_enabled = 0;
  divmmc_refresh_ptrs();
}


int divmmc_bank(void) {
  return self.bank_number;
}
