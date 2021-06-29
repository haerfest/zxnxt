#include <SDL2/SDL.h>
#include "log.h"
#include "mouse.h"


typedef struct {
  int   is_captured;
  u8_t  x;
  u8_t  y;
  u8_t  buttons;
} mouse_t;


static mouse_t self;


int mouse_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void mouse_finit(void) {
}


u8_t mouse_read_x(void) {
  return self.x;
}


u8_t mouse_read_y(void) {
  return self.y;
}


u8_t mouse_read_buttons(void) {
  return self.buttons;
}


void mouse_refresh(void) {
  if (self.is_captured) {
    int         dx;
    int         dy;
    const u32_t buttons = SDL_GetRelativeMouseState(&dx, &dy);
    const int   middle  = (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) ? 0: 4;
    const int   left    = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))   ? 0: 2;
    const int   right   = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))  ? 0: 1;

    self.buttons = middle | left | right;
    self.x       = (self.x + dx) & 0xFF;
    self.y       = (self.y - dy) % 0xFF;
  }
}


void mouse_toggle(void) {
  if (SDL_SetRelativeMouseMode(self.is_captured ? SDL_FALSE : SDL_TRUE) != 0) {
    log_err("mouse: SDL_SetRelativeMouseMode error: %s\n", SDL_GetError());
  }

  self.is_captured = (SDL_GetRelativeMouseMode() == SDL_TRUE);
}
