static u8_t ula_display_mode_screen_x(u32_t row, u32_t column) {
  const u32_t halved_column    = column / 2;
  const u8_t  mask             = 1 << (7 - (halved_column & 0x07));
  const u16_t attribute_offset = (row / 8) * 32 + halved_column / 8;
  const u8_t  attribute_byte   = self.attribute_ram[attribute_offset];
  const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((halved_column / 8) & 0x1F);
  const u8_t  display_byte     = self.display_ram[display_offset];
  const u8_t  ink              = PALETTE_OFFSET_INK   + (attribute_byte & 0x07);
  const u8_t  paper            = PALETTE_OFFSET_PAPER + (attribute_byte >> 3) & 0x07;
  const int   blink            = attribute_byte & 0x80;

  /* Turn the bright attribute into a palette offset of 8. */
  const u8_t  palette_index    = (attribute_byte & 0x40) >> 3;

  /* For floating-bus support. */
  self.attribute_byte = attribute_byte;

  return palette_index + (display_byte & mask
                          ? (blink && self.blink_state) ? paper : ink
                          : (blink && self.blink_state) ? ink : paper);
}
