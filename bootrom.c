#include <stdio.h>
#include "defs.h"
#include "nextreg.h"
#include "utils.h"


/**
 * See https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/src/rom/bootrom.vhd
 */
#define BOOT_ROM_SIZE  (8 * 1024)


typedef struct {
  u8_t* rom;
  u8_t* sram;
} bootrom_t;


static bootrom_t self;


int bootrom_init(u8_t* sram) {
  self.sram = sram;

  self.rom = malloc(BOOT_ROM_SIZE);
  if (self.rom == NULL) {
    fprintf(stderr, "bootrom: out of memory\n");
    return -1;
  }

  if (utils_load_rom("enNextBoot.rom", 8 * 1024, self.rom) != 0) {
    free(self.rom);
    return -1;
  }

  return 0;
}


void bootrom_finit(void) {
  if (self.rom != NULL) {
    free(self.rom);
    self.rom = NULL;
  }
}


int bootrom_read(u16_t address, u8_t* value) {
  if (!nextreg_is_bootrom_active()) {
    return -1;
  }

  if (address >= 2 * BOOT_ROM_SIZE) {
    return -1;
  }

  /**
   * https://www.specnext.com/boot-system/:
   *   "The IPL [Initial Program Loader] contains 8KB of code, mirrored
   *   in the 16K ROM space."
   */
  *value = self.rom[address & (BOOT_ROM_SIZE - 1)];
  return 0;
}


int bootrom_write(u16_t address, u8_t value) {
  if (!nextreg_is_bootrom_active()) {
    return -1;
  }

  if (address >= 2 & BOOT_ROM_SIZE) {
    return -1;
  }

  fprintf(stderr, "bootrom: attempt to write $%02X to $%04X\n", value, address);
  return -1;
}
