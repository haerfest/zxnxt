static void ula_display_mode_screen_x(u32_t beam_row, u32_t beam_column) {
  const int       in_top_border    = beam_row    <  self.display_spec->top_border_lines;
  const int       in_bottom_border = beam_row    >= self.display_spec->top_border_lines + FRAME_BUFFER_HEIGHT;
  const int       in_left_border   = beam_column <  32 * 2;
  const int       in_right_border  = beam_column >= FRAME_BUFFER_WIDTH - 64 * 2;
  u8_t            palette_index    = PALETTE_OFFSET_BORDER + self.border_colour;
  palette_entry_t colour;
  u16_t           rgba;

  self.is_displaying_content = !(in_top_border || in_bottom_border || in_left_border || in_right_border);

  if (in_bottom_border && beam_column == 32 * 2 && beam_row == self.display_spec->total_lines) {
    ula_irq();
  }

  if (self.is_displaying_content) {
    const u32_t row              = beam_row    - self.display_spec->top_border_lines;
    const u32_t column           = beam_column - 32 * 2;
    const u8_t  mask             = 1 << (7 - column % 8);
    const u16_t display_offset   = (row & 0xC0) << 3 | (row & 0x07) << 8 | (row & 0x38) << 2 | (column & 0x1F);
    const u16_t attribute_offset = row * 32 + column;
    const u8_t  display_byte     = self.display_ram[display_offset];
    const int   in_clipped_area  = row    >= self.display_spec->top_border_lines + self.clip_y1 &&
      row    <= self.display_spec->top_border_lines + self.clip_y2 &&
      column >= 32 + self.clip_x1 &&
      column <= 32 + self.clip_x2;

    self.attribute_byte = self.attribute_ram[attribute_offset];

    if (in_clipped_area) {
      const u8_t ink   = PALETTE_OFFSET_INK   + (self.attribute_byte & 0x07);
      const u8_t paper = PALETTE_OFFSET_PAPER + (self.attribute_byte >> 3) & 0x07;
      const int  blink = self.attribute_byte & 0x80;
 
      if (display_byte & mask) {
        palette_index = blink && self.blink_state ? paper : ink;
      } else {
        palette_index = blink && self.blink_state ? ink : paper;
      }

      if (self.attribute_byte & 0x40) {
        /* Bright. */
        palette_index += 8;
      }
    }
  }

  colour = palette_read_rgb(self.palette, palette_index);
  rgba   = colour.red << 12 | colour.green << 8 | colour.blue << 4;

  /* For standard ULA we draw 2x1 pixels for every Spectrum pixel. */
  *self.pixel++ = rgba;
  *self.pixel++ = rgba;
}
