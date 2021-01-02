#include <SDL2/SDL.h>
#include "defs.h"


#define N_ROWS  8
#define N_COLS  5


/* The column order is d0 - d4 in the response byte. */
const static SDL_Scancode scancodes[N_ROWS][N_COLS] = {
  /* 7F */ { SDL_SCANCODE_SPACE,  SDL_SCANCODE_LALT, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_B },
  /* BF */ { SDL_SCANCODE_RETURN, SDL_SCANCODE_L,    SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_H },
  /* DF */ { SDL_SCANCODE_P,      SDL_SCANCODE_O,    SDL_SCANCODE_I, SDL_SCANCODE_U, SDL_SCANCODE_Y },
  /* EF */ { SDL_SCANCODE_0,      SDL_SCANCODE_9,    SDL_SCANCODE_8, SDL_SCANCODE_7, SDL_SCANCODE_6 },
  /* F7 */ { SDL_SCANCODE_1,      SDL_SCANCODE_2,    SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5 },
  /* FB */ { SDL_SCANCODE_Q,      SDL_SCANCODE_W,    SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T },
  /* FD */ { SDL_SCANCODE_A,      SDL_SCANCODE_S,    SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G },
  /* FE */ { SDL_SCANCODE_LSHIFT, SDL_SCANCODE_Z,    SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V }
};


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
  const u8_t half_rows = ~(address >> 8);
  int        half_row;
  int        column;
  u8_t       mask;
  u8_t       pressed = 0x00;

  for (half_row = 0, mask = 0x80; mask != 0; half_row++, mask >>= 1) {
    if (half_rows & mask) {
      for (column = 0; column < N_COLS; column++) {
        const SDL_Scancode scancode = scancodes[half_row][column];
        pressed |= self.state[scancode] << column;
      }
    }
  }
  
  return ~pressed & 0x1F;
}
