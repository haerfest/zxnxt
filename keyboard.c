#include <SDL2/SDL.h>
#include "defs.h"
#include "log.h"


#define N_ROWS  8
#define N_COLS  5


/* All the physical keys on a ZX Spectrum 48K. */
typedef enum {
  E_KEY_1,          E_KEY_2, E_KEY_3, E_KEY_4, E_KEY_5, E_KEY_6, E_KEY_7, E_KEY_8, E_KEY_9,            E_KEY_0,
  E_KEY_Q,          E_KEY_W, E_KEY_E, E_KEY_R, E_KEY_T, E_KEY_Y, E_KEY_U, E_KEY_I, E_KEY_O,            E_KEY_P,
  E_KEY_A,          E_KEY_S, E_KEY_D, E_KEY_F, E_KEY_G, E_KEY_H, E_KEY_J, E_KEY_K, E_KEY_L,            E_KEY_ENTER,
  E_KEY_CAPS_SHIFT, E_KEY_Z, E_KEY_X, E_KEY_C, E_KEY_V, E_KEY_B, E_KEY_N, E_KEY_M, E_KEY_SYMBOL_SHIFT, E_KEY_SPACE,
} keyboard_key_t;


#define N_KEYS  (E_KEY_SPACE - E_KEY_1 + 1)


const static SDL_Scancode scancodes[N_KEYS] = {
  SDL_SCANCODE_1,      SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,    SDL_SCANCODE_0,
  SDL_SCANCODE_Q,      SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O,    SDL_SCANCODE_P,
  SDL_SCANCODE_A,      SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,    SDL_SCANCODE_RETURN,
  SDL_SCANCODE_LSHIFT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE
};


/* The column order is d0 - d4 in the response byte. */
const static keyboard_key_t matrix[N_ROWS][N_COLS] = {
  /* 7F */ { E_KEY_SPACE,      E_KEY_SYMBOL_SHIFT, E_KEY_M, E_KEY_N, E_KEY_B },
  /* BF */ { E_KEY_ENTER,      E_KEY_L,            E_KEY_K, E_KEY_J, E_KEY_H },
  /* DF */ { E_KEY_P,          E_KEY_O,            E_KEY_I, E_KEY_U, E_KEY_Y },
  /* EF */ { E_KEY_0,          E_KEY_9,            E_KEY_8, E_KEY_7, E_KEY_6 },
  /* F7 */ { E_KEY_1,          E_KEY_2,            E_KEY_3, E_KEY_4, E_KEY_5 },
  /* FB */ { E_KEY_Q,          E_KEY_W,            E_KEY_E, E_KEY_R, E_KEY_T },
  /* FD */ { E_KEY_A,          E_KEY_S,            E_KEY_D, E_KEY_F, E_KEY_G },
  /* FE */ { E_KEY_CAPS_SHIFT, E_KEY_Z,            E_KEY_X, E_KEY_C, E_KEY_V }
};


typedef struct {
  const u8_t* state;
  int         pressed[N_KEYS];
  int         capslock_on;
} self_t;


static self_t self;


int keyboard_init(void) {
  self.state       = SDL_GetKeyboardState(NULL);
  self.capslock_on = self.state[SDL_SCANCODE_CAPSLOCK];

  return 0;
}


void keyboard_finit(void) {
}


/* TODO: Use Caps Lock to trigger Caps Shift + 2. */
void keyboard_refresh(void) {
  int i;

  /* First capture the state of the physical keys on a ZX Spectrum 48K. */
  for (i = 0; i < N_KEYS; i++) {
    self.pressed[i] = self.state[scancodes[i]];
  }

  /* Also allow for the right modifier keys. */
  self.pressed[E_KEY_CAPS_SHIFT]   |= self.state[SDL_SCANCODE_RSHIFT];
  self.pressed[E_KEY_SYMBOL_SHIFT] |= self.state[SDL_SCANCODE_RALT];

  /* Then implement some of the additional keys on later models. */
  if (self.state[SDL_SCANCODE_BACKSPACE]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_0] = 1;
  }
  if (self.state[SDL_SCANCODE_GRAVE]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_1] = 1;
  }
  if (self.state[SDL_SCANCODE_LEFT]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_5] = 1;
  }
  if (self.state[SDL_SCANCODE_DOWN]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_6] = 1;
  }
  if (self.state[SDL_SCANCODE_UP]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_7] = 1;
  }
  if (self.state[SDL_SCANCODE_RIGHT]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_8] = 1;
  }
  if (self.state[SDL_SCANCODE_ESCAPE]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_SPACE] = 1;
  }
  if (self.state[SDL_SCANCODE_SEMICOLON]) {
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_O] = 1;
  }
    if (self.state[SDL_SCANCODE_APOSTROPHE]) {
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_P] = 1;
  }
  if (self.state[SDL_SCANCODE_COMMA]) {
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_N] = 1;
  }
  if (self.state[SDL_SCANCODE_PERIOD]) {
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_M] = 1;
  }
  if (self.state[SDL_SCANCODE_TAB]) {
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_SYMBOL_SHIFT] = 1;
  }
}


u8_t keyboard_read(u16_t address) {
  const u8_t half_rows = ~(address >> 8);
  int        half_row;
  int        column;
  u8_t       mask;
  u8_t       pressed = 0x00;

  keyboard_refresh();

  for (half_row = 0, mask = 0x80; mask != 0; half_row++, mask >>= 1) {
    if (half_rows & mask) {
      for (column = 0; column < N_COLS; column++) {
        const keyboard_key_t key = matrix[half_row][column];
        pressed |= self.pressed[key] << column;
      }
    }
  }
  
  return ~pressed & 0x1F;
}
