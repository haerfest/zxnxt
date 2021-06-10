#include <SDL2/SDL.h>
#include "log.h"
#include "mouse.h"


typedef struct {
  u8_t  x;
  u8_t  y;
  u8_t  buttons;
} mouse_t;


static mouse_t self;


int mouse_init(void) {
  if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0) {
    log_err("mouse: SDL_SetRelativeMouseMode error: %s\n", SDL_GetError());
    return -1;
  }

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
  int         dx;
  int         dy;
  const u32_t buttons = SDL_GetRelativeMouseState(&dx, &dy);
  const int   middle  = (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) ? 4 : 0;
  const int   left    = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))   ? 2 : 0;
  const int   right   = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))  ? 1 : 0;

  self.buttons = middle | left | right;
  self.x       = (self.x + dx) & 0xFF;
  self.y       = (self.y - dy) % 0xFF;
}
