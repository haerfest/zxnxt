static int ula_display_mode_screen_x(u32_t row, u32_t column, u8_t* palette_index) {
  const u32_t halved_column    = column / 2;
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const u16_t attribute_offset = (row / 8) * 32 + halved_column / 8;
  const u8_t  attribute_byte   = self.attribute_ram[attribute_offset];
  const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((halved_column / 8) & 0x1F);
  const u8_t  display_byte     = self.display_ram[display_offset];
  const int   is_foreground    = display_byte & mask;

  u8_t ink;
  u8_t paper;
  u8_t blink;
  u8_t bright;

  /* For floating-bus support. */
  self.attribute_byte = attribute_byte;

  if (self.is_ula_next_mode) {
    if (!is_foreground && self.ula_next_mask_paper == 0) {
      /* No background mask, background color is fallback color. */
      return 0;
    }

    /* TODO: shift paper palette index to the right! */
    ink    = 0   + (attribute_byte & self.ula_next_mask_ink);
    paper  = 128 + (attribute_byte & self.ula_next_mask_paper);
    blink  = 0;
    bright = 0;
  } else {
    bright = (attribute_byte & 0x40) >> 3;
    ink    = 0  + bright + (attribute_byte & 0x07);
    paper  = 16 + bright + ((attribute_byte >> 3) & 0x07);
    blink  = attribute_byte & 0x80;
  }

  *palette_index = is_foreground
    ? ((blink && self.blink_state) ? paper : ink)
    : ((blink && self.blink_state) ? ink : paper);

  return 1;
}
