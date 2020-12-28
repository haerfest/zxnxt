#include <stdio.h>
#include "defs.h"
#include "memory.h"
#include "nextreg.h"
#include "rom.h"
#include "utils.h"


typedef enum {
  E_ALTROM_ACTIVE_READ,
  E_ALTROM_ACTIVE_WRITE
} altrom_active_t;


typedef enum {
  E_ROM_ACTION_READ,
  E_ROM_ACTION_WRITE
} rom_action_t;


typedef struct {
  u8_t*           sram;
  u8_t*           altroms[2];
  u8_t            selected;
  int             altrom_enabled;
  altrom_active_t altrom_active;
  u8_t            lock_rom;
} rom_t;


static rom_t self;


int rom_init(u8_t* sram) {
  self.sram           = sram;
  self.altroms[0]     = &sram[MEMORY_RAM_OFFSET_ALTROM0_128K];
  self.altroms[1]     = &sram[MEMORY_RAM_OFFSET_ALTROM1_48K];
  self.altrom_enabled = 0;
  self.altrom_active  = E_ALTROM_ACTIVE_WRITE;
  self.lock_rom       = 0;
  self.selected       = 0;

  return 0;
}


void rom_finit(void) {
}


static int rom_is_altrom_active(rom_action_t action) {
  if (!self.altrom_enabled) {
    return 0;
  }

  if (action == E_ROM_ACTION_READ && self.altrom_active == E_ALTROM_ACTIVE_READ) {
    return 1;
  }

  if (action == E_ROM_ACTION_WRITE && self.altrom_active == E_ALTROM_ACTIVE_WRITE) {
    return 1;
  }

  return 0;
}


static int rom_translate_48k(u16_t address, u8_t** sram, rom_action_t action) {
  if (rom_is_altrom_active(action)) {
    const u8_t select128 = self.lock_rom == 0x01;
    *sram = &self.altroms[1 - select128][address];
    return 0;
  }

  *sram = &self.sram[address];  /* Always ROM 00??? */
  return 0;
}


static int rom_translate_128k(u16_t address, u8_t** sram, rom_action_t action) {
  fprintf(stderr, "rom: address $%04X translation error, ZX 128K/PLUS2 not implemented yet\n", address);
  return -1;
}


static int rom_translate_plus3(u16_t address, u8_t** sram, rom_action_t action) {
  if (rom_is_altrom_active(action)) {
    const u8_t select128 = (self.lock_rom == 0) ? 1 - (self.selected & 0x01) : 1 - (self.lock_rom >> 1);
    *sram = &self.altroms[1 - select128][address];
    return 0;
  }

  if (action == E_ROM_ACTION_WRITE) {
    fprintf(stderr, "rom: attempt to write to ROM address $%04X\n", address);
    return -1;
  }

  *sram = &self.sram[rom_selected() * 16 * 1024 + address];
  return 0;
}


static int rom_translate(u16_t address, u8_t** sram, rom_action_t action) {
  const u8_t machine_type = nextreg_get_machine_type();

  switch (machine_type) {
    case E_NEXTREG_MACHINE_TYPE_ZX_48K:
      return rom_translate_48k(address, sram, action);

    case E_NEXTREG_MACHINE_TYPE_ZX_128K_PLUS2:
    case E_NEXTREG_MACHINE_TYPE_PENTAGON:
      return rom_translate_128k(address, sram, action);

    case E_NEXTREG_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3:
      return rom_translate_plus3(address, sram, action);

    default:
      fprintf(stderr, "rom: address $%04X translation error for machine type (%u)\n", address, machine_type);
      return -1;
  }
}


int rom_read(u16_t address, u8_t* value) {
  u8_t* sram;

  if (rom_translate(address, &sram, E_ROM_ACTION_READ) != 0) {
    return -1;
  }

  *value = *sram;
  return 0;
}


int rom_write(u16_t address, u8_t value) {
  u8_t* sram;

  if (rom_translate(address, &sram, E_ROM_ACTION_WRITE) != 0) {
    return -1;
  }

  *sram = value;
  return 0;
}


void rom_select(u8_t rom) {
  self.selected = rom & 0x03;
}


u8_t rom_selected(void) {
  return (self.lock_rom == 0) ? self.selected : self.lock_rom;
}


void rom_configure_altrom(int enable, int during_writes, u8_t lock_rom) {
  self.altrom_enabled = enable;
  self.altrom_active  = during_writes ? E_ALTROM_ACTIVE_WRITE : E_ALTROM_ACTIVE_READ;
  self.lock_rom       = lock_rom & 0x03;
}
