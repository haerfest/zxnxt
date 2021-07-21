#include <stdio.h>
#include "defs.h"
#include "log.h"
#include "memory.h"
#include "nextreg.h"
#include "utils.h"


/**
 * See https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/rom/bootrom.vhd
 */
#define BOOT_ROM_SIZE  (8 * 1024)


typedef struct bootrom_t {
  u8_t* rom;
  u8_t* sram;
  int   is_active;
} bootrom_t;


static bootrom_t self;


int bootrom_init(u8_t* sram) {
  self.sram = sram;

  self.rom = malloc(BOOT_ROM_SIZE);
  if (self.rom == NULL) {
    log_err("bootrom: out of memory\n");
    return -1;
  }

  if (utils_load_rom("enNextBoot.rom", 8 * 1024, self.rom) != 0) {
    free(self.rom);
    return -1;
  }

  self.is_active = 1;
  return 0;
}


void bootrom_finit(void) {
  if (self.rom != NULL) {
    free(self.rom);
    self.rom = NULL;
  }
}


int bootrom_is_active(void) {
  return self.is_active;
}


void bootrom_activate(void) {
  const int was_active = self.is_active;

  self.is_active = 1;

  if (!was_active) {
    memory_refresh_accessors(0, 2);
  }
}


void bootrom_deactivate(void) {
  const int was_active = self.is_active;
  
  self.is_active = 0;

  if (was_active) {
    memory_refresh_accessors(0, 2);
  }
}


u8_t bootrom_read(u16_t address) {
  /**
   * https://www.specnext.com/boot-system/:
   *   "The IPL [Initial Program Loader] contains 8KB of code, mirrored
   *   in the 16K ROM space."
   */
  return self.rom[address & (BOOT_ROM_SIZE - 1)];
}


void bootrom_write(u16_t address, u8_t value) {
}
