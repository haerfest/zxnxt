static void ula_display_mode_hi_res(u32_t beam_row, u32_t beam_column) {

  const u32_t frame_buffer_row    = beam_row    - self.display_spec->row_border_top;
  const u32_t frame_buffer_column = beam_column - self.display_spec->column_border_left;

  u8_t            palette_index  = PALETTE_OFFSET_BORDER + self.border_colour;
  palette_entry_t colour;
  u16_t           rgba;

  self.is_displaying_content = \
    beam_row    >= self.display_spec->row_content       &&
    beam_row    <  self.display_spec->row_border_bottom &&
    beam_column >= self.display_spec->column_content    &&
    beam_column <  self.display_spec->column_border_right;

  if (self.is_displaying_content) {
    const u32_t row             = beam_row    - self.display_spec->row_content;
    const u32_t column          = beam_column - self.display_spec->column_content;
    const int   in_clipped_area = \
      row    >= self.clip_y1 && row    <= self.clip_y2 &&
      column >= self.clip_x1 && column <= self.clip_x2;

    if (in_clipped_area) {
      const u8_t  mask             = 1 << (7 - (column & 0x07));
      const u16_t display_offset   = ((row & 0xC0) << 5) | ((row & 0x07) << 8) | ((row & 0x38) << 2) | (((column >> 1) / 8) & 0x1F);;
      const u8_t* display_ram      = ((column / 8) & 0x01) ? self.display_ram_odd : self.display_ram;
      const u8_t  display_byte     = display_ram[display_offset];

      if (display_byte & mask) {
        palette_index = PALETTE_OFFSET_INK   + 8 + self.hi_res_ink_colour;
      } else {
        palette_index = PALETTE_OFFSET_PAPER + 8 + (~self.hi_res_ink_colour & 0x07);
      }
    }
  }

  colour = palette_read_rgb(self.palette, palette_index);
  rgba   = colour.red << 12 | colour.green << 8 | colour.blue << 4;

  self.frame_buffer[frame_buffer_row * FRAME_BUFFER_WIDTH + frame_buffer_column] = rgba;
}
