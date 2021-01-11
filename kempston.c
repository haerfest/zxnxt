#include <SDL2/SDL.h>
#include "defs.h"


typedef struct {
  SDL_GameController* controller;
  u8_t                state;
} self_t;


static self_t self;


int kempston_init(SDL_GameController* controller) {
  self.controller = controller;
  self.state      = 0;
  return 0;
}


void kempston_finit(void) {
}


u8_t kempston_read(u16_t address) {
  return self.state;
}


void kempston_refresh(void) {
  self.state = 0;

  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) self.state |= 0x01;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  self.state |= 0x02;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  self.state |= 0x04;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    self.state |= 0x08;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_A))          self.state |= 0x10;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_B))          self.state |= 0x20;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_X))          self.state |= 0x40;
  if (SDL_GameControllerGetButton(self.controller, SDL_CONTROLLER_BUTTON_START))      self.state |= 0x80;
}
