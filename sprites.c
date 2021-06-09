#include <string.h>
#include "sprites.h"


typedef struct {
  int  is_enabled;
  int  is_enabled_over_border;
  int  is_enabled_clipping_over_border;
  int  is_in_lockstep;
  int  is_zero_on_top;
  int  do_clip_in_over_border_mode;
  int  clip_x1;
  int  clip_x2;
  int  clip_y1;
  int  clip_y2;
  u8_t transparency_index;
} sprites_t;


static sprites_t self;


int sprites_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void sprites_finit(void) {
}


void sprites_tick(u32_t row, u32_t column, int* is_transparent, u16_t* rgba) {
  *is_transparent = 1;
}


void sprites_lockstep_set(int enable) {
  self.is_in_lockstep = enable;
}


void sprites_priority_set(int is_zero_on_top) {
  self.is_zero_on_top = is_zero_on_top;
}


void sprites_enable_set(int enable) {
  self.is_enabled = enable;
}


void sprites_enable_over_border_set(int enable) {
  self.is_enabled_over_border = enable;
}


void sprites_enable_clipping_over_border_set(int enable) {
  self.is_enabled_clipping_over_border = enable;
}


void sprites_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2) {
  self.clip_x1 = x1;
  self.clip_x2 = x2;
  self.clip_y1 = y1;
  self.clip_y2 = y2;
}


void sprites_attributes_set(u8_t number, u8_t attribute_1, u8_t attribute_2, u8_t attribute_3, u8_t attribute_4, u8_t attribute_5) {
}


void sprites_transparency_index_write(u8_t value) {
  self.transparency_index = value;
}
