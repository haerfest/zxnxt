static void ula_display_mode_screen_x_top_border(void);
static void ula_display_mode_screen_x_bottom_border(void);
static void ula_display_mode_screen_x_left_border(void);
static void ula_display_mode_screen_x_right_border(void);
static void ula_display_mode_screen_x_content(void);
static void ula_display_mode_screen_x_hsync(void);
static void ula_display_mode_screen_x_vsync(void);


static void ula_display_mode_x_plot(palette_entry_t colour) {
  const u16_t RGBA = colour.red << 12 | colour.green << 8 | colour.blue << 4;

  /* For standard ULA we draw 2x1 pixels for every Spectrum pixel. */
  *self.pixel++ = RGBA;
  *self.pixel++ = RGBA;
}


static void ula_display_mode_screen_x_top_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_x_plot(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_mode_handler = ula_display_mode_screen_x_hsync;
  }
}


static void ula_display_mode_screen_x_left_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_x_plot(colour);

  if (++self.display_column == 32) {
    const u16_t line = self.display_line - self.display_spec->top_border_lines;
    self.display_offset        = self.display_offsets[line];
    self.attribute_offset      = self.attribute_offsets[line];
    self.display_mode_handler = ula_display_mode_screen_x_content;
  }
}


static void ula_display_mode_screen_x_content(void) {
  palette_entry_t colour;
  u8_t            index;
  
  if (self.display_pixel_mask == 0x00) {
    self.display_pixel_mask = 0x80;
    self.display_byte       = self.display_ram[self.display_offset++];
    self.attribute_byte     = self.attribute_ram[self.attribute_offset++];
  }

  if (self.display_line   >= self.display_spec->top_border_lines + self.clip_y1 &&
      self.display_line   <= self.display_spec->top_border_lines + self.clip_y2 &&
      self.display_column >= 32 + self.clip_x1 &&
      self.display_column <= 32 + self.clip_x2) {

    const u8_t ink   = PALETTE_OFFSET_INK   + (self.attribute_byte & 0x07);
    const u8_t paper = PALETTE_OFFSET_PAPER + (self.attribute_byte >> 3) & 0x07;
    const int  blink = self.attribute_byte & 0x80;
 
    if (self.display_byte & self.display_pixel_mask) {
      index = blink && self.blink_state ? paper : ink;
    } else {
      index = blink && self.blink_state ? ink : paper;
    }
    if (self.attribute_byte & 0x40) {
      /* Bright. */
      index += 8;
    }
  } else {
    index = PALETTE_OFFSET_BORDER + self.border_colour;
  }

  colour = palette_read_rgb(self.palette, index);
  ula_display_mode_x_plot(colour);

  self.display_pixel_mask >>= 1;
  if (++self.display_column == 32 + 256) {
    self.display_mode_handler = ula_display_mode_screen_x_right_border;
  }
}


static void ula_display_mode_screen_x_hsync(void) {
  if (++self.display_column == 32 + 256 + 64 + 96) {
    self.display_column = 0;
    self.display_line++;

    if (self.display_line < self.display_spec->top_border_lines) {
      self.display_mode_handler = ula_display_mode_screen_x_top_border;
    } else if (self.display_line < self.display_spec->top_border_lines + self.display_spec->display_lines) {
      self.display_mode_handler = ula_display_mode_screen_x_left_border;
    } else if (self.display_line < self.display_spec->total_lines) {
      self.display_mode_handler = ula_display_mode_screen_x_bottom_border;
    } else {
      self.display_mode_handler = ula_display_mode_screen_x_vsync;
    }
  }
}


static void ula_display_mode_screen_x_right_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_x_plot(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_mode_handler = ula_display_mode_screen_x_hsync;
  }
}


static void ula_display_mode_screen_x_bottom_border(void) {
  const palette_entry_t colour = palette_read_rgb(self.palette, PALETTE_OFFSET_BORDER + self.border_colour);

  ula_display_mode_x_plot(colour);

  if (++self.display_column == 32 + 256 + 64) {
    self.display_mode_handler = ula_display_mode_screen_x_hsync;
  }
}


static void ula_display_mode_screen_x_vsync(void) {
  self.display_column++;

  if (self.display_column == 32) {
    if (self.display_line == self.display_spec->total_lines) {
      ula_irq();
    }
  } else if (self.display_column == 32 + 256 + 64 + 96) {
    self.display_column = 0;
    self.display_line++;

    if (self.display_line == self.display_spec->total_lines + self.display_spec->blanking_period_lines) {
      self.display_line         = 0;
      self.display_offset       = 0;
      self.attribute_offset     = 0;
      self.display_mode_handler = ula_display_mode_screen_x_top_border;

      ula_frame_complete();
    }
  }
}
