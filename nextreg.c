#include <stdio.h>
#include "memory.h"
#include "mmu.h"
#include "defs.h"


#define REGISTER_MMU_SLOT0_CONTROL       0x50
#define REGISTER_MMU_SLOT1_CONTROL       0x51
#define REGISTER_MMU_SLOT2_CONTROL       0x52
#define REGISTER_MMU_SLOT3_CONTROL       0x53
#define REGISTER_MMU_SLOT4_CONTROL       0x54
#define REGISTER_MMU_SLOT5_CONTROL       0x55
#define REGISTER_MMU_SLOT6_CONTROL       0x56
#define REGISTER_MMU_SLOT7_CONTROL       0x57
#define REGISTER_SPECTRUM_MEMORY_MAPPING 0x8E
#define REGISTER_MEMORY_MAPPING_MODE     0x8F


typedef struct {
  u8_t selected_register;
} nextreg_t;


static nextreg_t nextreg;


int nextreg_init(void) {
  return 0;
}


void nextreg_finit(void) {
}


void nextreg_select_write(u8_t value) {
  nextreg.selected_register = value;
}


u8_t nextreg_select_read(void) {
  return 0xFF;
}


void nextreg_data_write(u8_t value) {
  switch (nextreg.selected_register) {
    case REGISTER_MMU_SLOT0_CONTROL:
    case REGISTER_MMU_SLOT1_CONTROL:
    case REGISTER_MMU_SLOT2_CONTROL:
    case REGISTER_MMU_SLOT3_CONTROL:
    case REGISTER_MMU_SLOT4_CONTROL:
    case REGISTER_MMU_SLOT5_CONTROL:
    case REGISTER_MMU_SLOT6_CONTROL:
    case REGISTER_MMU_SLOT7_CONTROL:
      mmu_page_write(nextreg.selected_register - REGISTER_MMU_SLOT0_CONTROL, value);
      break;

    case REGISTER_SPECTRUM_MEMORY_MAPPING:
      memory_spectrum_memory_mapping_write(value);
      break;

    case REGISTER_MEMORY_MAPPING_MODE:
      memory_mapping_mode_write(value);
      break;

    default:
      fprintf(stderr, "Unimplemented write %02Xh to Next register %02Xh\n", value, nextreg.selected_register);
      break;
  }
}


u8_t nextreg_data_read(void) {
  switch (nextreg.selected_register) {
    case REGISTER_MMU_SLOT0_CONTROL:
    case REGISTER_MMU_SLOT1_CONTROL:
    case REGISTER_MMU_SLOT2_CONTROL:
    case REGISTER_MMU_SLOT3_CONTROL:
    case REGISTER_MMU_SLOT4_CONTROL:
    case REGISTER_MMU_SLOT5_CONTROL:
    case REGISTER_MMU_SLOT6_CONTROL:
    case REGISTER_MMU_SLOT7_CONTROL:
      return mmu_page_read(nextreg.selected_register - REGISTER_MMU_SLOT0_CONTROL);

    default:
      fprintf(stderr, "Unimplemented read from Next register %02Xh\n", nextreg.selected_register);
      break;
  }

  return 0xFF;
}


