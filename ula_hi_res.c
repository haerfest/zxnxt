static void ula_display_mode_hi_res(u32_t beam_row, u32_t beam_column) {
}


#if 0

static void ula_display_mode_hi_res_plot(palette_entry_t colour) {
  const u16_t RGBA = colour.red << 12 | colour.green << 8 | colour.blue << 4;
  *self.pixel++ = RGBA;
}


static void ula_display_mode_hi_res_top_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_hi_res_plot(colour);

  if (++self.display_column == (32 + 256 + 64) * 2) {
    self.display_phase = E_ULA_DISPLAY_PHASE_HORIZONTAL_SYNC;
  }
}


static void ula_display_mode_hi_res_left_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_hi_res_plot(colour);

  if (++self.display_column == 32 * 2) {
    const u16_t line = self.display_line - self.display_spec->top_border_lines;
    self.display_offset = self.display_offsets[line];
    self.display_phase  = E_ULA_DISPLAY_PHASE_CONTENT;
  }
}


static void ula_display_mode_hi_res_content(void) {
  palette_entry_t colour;
  u8_t            index;
  
  if (self.display_pixel_mask == 0x00) {
    const int   is_odd_column = self.display_column & 0x08;
    const u8_t* display_ram   = is_odd_column ? self.display_ram_odd : self.display_ram;
    self.display_pixel_mask = 0x80;
    self.display_byte       = display_ram[self.display_offset];

    if (is_odd_column) {
      self.display_offset++;
    }
  }

  if (self.display_line   >= self.display_spec->top_border_lines + self.clip_y1 &&
      self.display_line   <= self.display_spec->top_border_lines + self.clip_y2 &&
      self.display_column >= (32 + self.clip_x1) * 2 &&
      self.display_column <= (32 + self.clip_x2) * 2) {
    if (self.display_byte & self.display_pixel_mask) {
      index = PALETTE_OFFSET_INK   + 8 + self.hi_res_ink_colour;
    } else {
      index = PALETTE_OFFSET_PAPER + 8 + (~self.hi_res_ink_colour & 0x07);
    }
  } else {
    index = PALETTE_OFFSET_BORDER + self.border_colour;
  }

  colour = palette_read_rgb(self.palette, index);
  ula_display_mode_hi_res_plot(colour);

  self.display_pixel_mask >>= 1;
  if (++self.display_column == (32 + 256) * 2) {
    self.display_phase = E_ULA_DISPLAY_PHASE_RIGHT_BORDER;
  }
}


static void ula_display_mode_hi_res_hsync(void) {
  if (++self.display_column == (32 + 256 + 64 + 96) * 2) {
    self.display_column = 0;
    self.display_line++;

    if (self.display_line < self.display_spec->top_border_lines) {
      self.display_phase = E_ULA_DISPLAY_PHASE_TOP_BORDER;
    } else if (self.display_line < self.display_spec->top_border_lines + self.display_spec->display_lines) {
      self.display_phase = E_ULA_DISPLAY_PHASE_LEFT_BORDER;
    } else if (self.display_line < self.display_spec->total_lines) {
      self.display_phase = E_ULA_DISPLAY_PHASE_BOTTOM_BORDER;
    } else {
      self.display_phase = E_ULA_DISPLAY_PHASE_VERTICAL_SYNC;
    }
  }
}


static void ula_display_mode_hi_res_right_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_hi_res_plot(colour);

  if (++self.display_column == (32 + 256 + 64) * 2) {
    self.display_phase = E_ULA_DISPLAY_PHASE_HORIZONTAL_SYNC;
  }
}


static void ula_display_mode_hi_res_bottom_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_hi_res_plot(colour);

  if (++self.display_column == (32 + 256 + 64) * 2) {
    self.display_phase = E_ULA_DISPLAY_PHASE_HORIZONTAL_SYNC;
  }
}


static void ula_display_mode_hi_res_vsync(void) {
  self.display_column++;

  if (self.display_column == 32 * 2) {
    if (self.display_line == self.display_spec->total_lines) {
      ula_irq();
    }
  } else if (self.display_column == (32 + 256 + 64 + 96) * 2) {
    self.display_column = 0;
    self.display_line++;

    if (self.display_line == self.display_spec->total_lines + self.display_spec->blanking_period_lines) {
      self.display_line   = 0;
      self.display_offset = 0;
      self.display_phase  = E_ULA_DISPLAY_PHASE_TOP_BORDER;

      ula_frame_complete();
    }
  }
}
#endif
