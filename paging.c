#include "defs.h"
#include "log.h"
#include "mmu.h"
#include "paging.h"
#include "rom.h"
#include "ula.h"


typedef struct {
  int  is_spectrum_128k_paging_locked;
  u8_t bank_at_C000;
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
  self.bank_at_C000                   = 0;
}


void paging_unlock_spectrum_128k_paging(void) {
  self.is_spectrum_128k_paging_locked = 0;
}


u8_t paging_spectrum_128k_paging_read(void) {
  return self.is_spectrum_128k_paging_locked            << 5 |
         (rom_selected() & 0x01)                        << 4 |
         (ula_screen_bank_get() == E_ULA_SCREEN_BANK_7) << 3 |
         (self.bank_at_C000 & 0x03);
}


void paging_spectrum_128k_paging_write(u8_t value) {
  if (self.is_spectrum_128k_paging_locked) {
    log_dbg("paging: cannot write $%02X to locked Spectrum 128K paging port\n");
    return;
  }
  
  /* TODO: Implement 16K RAM bank paging Pentagon 512K mode. */

  rom_select((rom_selected() & ~0x01) | (value & 0x10) >> 4);
  ula_screen_bank_set((value & 0x08) ? E_ULA_SCREEN_BANK_7 : E_ULA_SCREEN_BANK_5);

  self.bank_at_C000 = (self.bank_at_C000 & ~0x03) | (value & 0x03);
  mmu_bank_set(4, self.bank_at_C000);

  if (value & 0x20) {
    self.is_spectrum_128k_paging_locked = 1;
  }
}


u8_t paging_spectrum_next_bank_extension_read(void) {
  return 0xFF;
}


void paging_spectrum_next_bank_extension_write(u8_t value) {
}


u8_t paging_spectrum_plus_3_paging_read(void) {
  return 0xFF;
}


void paging_spectrum_plus_3_paging_write(u8_t value) {
}
