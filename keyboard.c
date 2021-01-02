#include <SDL2/SDL.h>
#include "defs.h"


typedef struct {
  const u8_t* state;
} self_t;


static self_t self;


int keyboard_init(void) {
  self.state = SDL_GetKeyboardState(NULL);
  return 0;
}


void keyboard_finit(void) {
}


u8_t keyboard_read(u16_t address) {
  return 0x1F;
}
