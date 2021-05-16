static void ula_display_mode_screen_x(u32_t beam_row, u32_t beam_column) {
  /* The frame buffer is twice as wide as the normal ULA resolution. */
  const u32_t     half_column      = beam_column / 2;
  const int       in_top_border    = beam_row    <  self.display_spec->top_border_lines;
  const int       in_bottom_border = beam_row    >= FRAME_BUFFER_HEIGHT - self.display_spec->bottom_border_lines;
  const int       in_left_border   = half_column <  32;
  const int       in_right_border  = half_column >= FRAME_BUFFER_WIDTH / 2 - 32;
  u8_t            palette_index    = PALETTE_OFFSET_BORDER + self.border_colour;
  palette_entry_t colour;
  u16_t           rgba;

  self.is_displaying_content = !(in_top_border || in_bottom_border || in_left_border || in_right_border);

  /* TODO: the ULA IRQ is fired at a different beam location. */
  if (beam_row == FRAME_BUFFER_HEIGHT - 1 && half_column == FRAME_BUFFER_WIDTH / 2 - 1) {
    ula_irq();
  }

  if (self.is_displaying_content) {
    const u32_t row             = beam_row    - self.display_spec->top_border_lines;
    const u32_t column          = half_column - 32;
    const int   in_clipped_area = row    >= self.clip_y1 && row    <= self.clip_y2 &&
                                  column >= self.clip_x1 && column <= self.clip_x2;

    if (in_clipped_area) {
      const u8_t  mask             = 1 << (7 - (column & 0x07));
      const u16_t attribute_offset = (row / 8) * 32 + column / 8;
      const u8_t  attribute_byte   = self.attribute_ram[attribute_offset];
      const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | ((column / 8) & 0x1F);
      const u8_t  display_byte     = self.display_ram[display_offset];
      const u8_t  ink              = PALETTE_OFFSET_INK   + (attribute_byte & 0x07);
      const u8_t  paper            = PALETTE_OFFSET_PAPER + (attribute_byte >> 3) & 0x07;
      const int   blink            = attribute_byte & 0x80;

      if (display_byte & mask) {
        palette_index = blink && self.blink_state ? paper : ink;
      } else {
        palette_index = blink && self.blink_state ? ink : paper;
      }

      if (attribute_byte & 0x40) {
        /* Bright. */
        palette_index += 8;
      }

      /* For floating-bus support. */
      self.attribute_byte = attribute_byte;
    }
  }

  colour = palette_read_rgb(self.palette, palette_index);
  rgba   = colour.red << 12 | colour.green << 8 | colour.blue << 4;

  /* For standard ULA we draw 2x1 pixels for every Spectrum pixel. */
  self.frame_buffer[beam_row * FRAME_BUFFER_WIDTH + beam_column + 0] = rgba;
  self.frame_buffer[beam_row * FRAME_BUFFER_WIDTH + beam_column + 1] = rgba;
}
