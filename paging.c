#include "defs.h"
#include "log.h"
#include "mmu.h"
#include "paging.h"
#include "rom.h"
#include "ula.h"


typedef struct {
  int  is_spectrum_128k_paging_locked;
  u8_t bank_slot_4;
} self_t;


static self_t self;


int paging_init(void) {
  paging_reset();
  return 0;
}


void paging_finit(void) {
}


void paging_reset(void) {
  self.is_spectrum_128k_paging_locked = 0;
  self.bank_slot_4                    = 0;
}


void paging_spectrum_128k_ram_bank_slot_4_set(u8_t bank) {
  self.bank_slot_4 = bank;
  mmu_bank_set(4, self.bank_slot_4);
}


int paging_spectrum_128k_paging_is_locked(void) {
  return self.is_spectrum_128k_paging_locked;
}


void paging_spectrum_128k_paging_unlock(void) {
  self.is_spectrum_128k_paging_locked = 0;
}


u8_t paging_spectrum_128k_paging_read(void) {
  return self.is_spectrum_128k_paging_locked            << 5 |
         (rom_selected() & 0x01)                        << 4 |
         (ula_screen_bank_get() == E_ULA_SCREEN_BANK_7) << 3 |
         (self.bank_slot_4 & 0x03);
}


/**
 * https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/ports.txt
 *
 * "A write to any of the ports 0x7FFD, 0xDFFD, 0x1FFD, 0xEFF7 sets mmu0=0xff
 * and mmu1=0xff to reveal the selected rom in the bottom 16K.  mmu6 and mmu7
 * are set to reflect the selected 16K memory bank."
 */


static void paging_mmu_update(void) {
  mmu_page_set(0, MMU_ROM_PAGE);
  mmu_page_set(1, MMU_ROM_PAGE);
  mmu_bank_set(4, self.bank_slot_4);
}


void paging_spectrum_128k_paging_write(u8_t value) {
  if (self.is_spectrum_128k_paging_locked) {
    return;
  }

  /* TODO: Implement 16K RAM bank paging Pentagon 512K mode. */

  rom_select((rom_selected() & ~0x01) | (value & 0x10) >> 4);
  ula_screen_bank_set((value & 0x08) ? E_ULA_SCREEN_BANK_7 : E_ULA_SCREEN_BANK_5);

  self.bank_slot_4 = (self.bank_slot_4 & ~0x07) | (value & 0x07);

  if (value & 0x20) {
    self.is_spectrum_128k_paging_locked = 1;
  }

  paging_mmu_update();
}


u8_t paging_spectrum_next_bank_extension_read(void) {
  /* Is write-only. */
  return 0xFF;
}


void paging_spectrum_next_bank_extension_write(u8_t value) {
  self.bank_slot_4 = (value & 0x0F) << 3 | (self.bank_slot_4 & 0x07);
  paging_mmu_update();
}


u8_t paging_spectrum_plus_3_paging_read(void) {
  return (rom_selected() & 0x02) << 1;
}


void paging_spectrum_plus_3_paging_write(u8_t value) {
  const u8_t is_special = value & 0x01;

  if (is_special) {
    paging_all_ram((value >> 1) & 0x03);
  } else {
    const u8_t rom = (value & 0x04) >> 1 | (rom_selected() & 0x01);
    rom_select(rom);
    paging_mmu_update();
  }
}


void paging_all_ram(u8_t value) {
  const u8_t banks[4][4] = {
    {0, 1, 2, 3},
    {4, 5, 6, 7},
    {4, 5, 6, 3},
    {4, 7, 6, 3}
  };

  log_dbg("paging: ALL RAM %d\n", value);

  if (value > 3) {
    log_wrn("paging: invalid all-ram setting %d\n", value);
    return;
  }

  mmu_bank_set(1, banks[value][0]);
  mmu_bank_set(2, banks[value][1]);
  mmu_bank_set(3, banks[value][2]);
  mmu_bank_set(4, banks[value][3]);
}
