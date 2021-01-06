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


typedef struct {
  const u8_t* state;
  u8_t        half_row_FE;
  u8_t        half_row_FD;
  u8_t        half_row_FB;
  u8_t        half_row_F7;
  u8_t        half_row_EF;
  u8_t        half_row_DF;
  u8_t        half_row_BF;
  u8_t        half_row_7F;
} self_t;


static self_t self;


int keyboard_init(void) {
  self.state       = SDL_GetKeyboardState(NULL);
  self.half_row_FE = 0;
  self.half_row_FD = 0;
  self.half_row_FB = 0;
  self.half_row_F7 = 0;
  self.half_row_EF = 0;
  self.half_row_DF = 0;
  self.half_row_BF = 0;
  self.half_row_7F = 0;

  return 0;
}


void keyboard_finit(void) {
}


/* TODO: Use Caps Lock to trigger Caps Shift + 2. */
void keyboard_refresh(void) {
  int p[N_KEYS];  /* 'p' = pressed */
  int i;

  /* First capture the state of the physical keys on a ZX Spectrum 48K. */
  for (i = 0; i < N_KEYS; i++) {
    p[i] = self.state[scancodes[i]];
  }

  /* Also allow for the right modifier keys. */
  p[E_KEY_CAPS_SHIFT]   |= self.state[SDL_SCANCODE_RSHIFT];
  p[E_KEY_SYMBOL_SHIFT] |= self.state[SDL_SCANCODE_RALT];

  /* Then implement some of the additional keys on later models. */
  if (self.state[SDL_SCANCODE_BACKSPACE]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_0] = 1;
  }
  if (self.state[SDL_SCANCODE_GRAVE]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_1] = 1;
  }
  if (self.state[SDL_SCANCODE_LEFT]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_5] = 1;
  }
  if (self.state[SDL_SCANCODE_DOWN]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_6] = 1;
  }
  if (self.state[SDL_SCANCODE_UP]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_7] = 1;
  }
  if (self.state[SDL_SCANCODE_RIGHT]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_8] = 1;
  }
  if (self.state[SDL_SCANCODE_ESCAPE]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_SPACE] = 1;
  }
  if (self.state[SDL_SCANCODE_SEMICOLON]) {
    p[E_KEY_SYMBOL_SHIFT] = p[E_KEY_O] = 1;
  }
    if (self.state[SDL_SCANCODE_APOSTROPHE]) {
    p[E_KEY_SYMBOL_SHIFT] = p[E_KEY_P] = 1;
  }
  if (self.state[SDL_SCANCODE_COMMA]) {
    p[E_KEY_SYMBOL_SHIFT] = p[E_KEY_N] = 1;
  }
  if (self.state[SDL_SCANCODE_PERIOD]) {
    p[E_KEY_SYMBOL_SHIFT] = p[E_KEY_M] = 1;
  }
  if (self.state[SDL_SCANCODE_TAB]) {
    p[E_KEY_CAPS_SHIFT] = p[E_KEY_SYMBOL_SHIFT] = 1;
  }

  self.half_row_FE = (p[E_KEY_CAPS_SHIFT] << 0) | (p[E_KEY_Z] << 1) | (p[E_KEY_X] << 2) | (p[E_KEY_C]            << 3) | (p[E_KEY_V]     << 4);
  self.half_row_FD = (p[E_KEY_A]          << 0) | (p[E_KEY_S] << 1) | (p[E_KEY_D] << 2) | (p[E_KEY_F]            << 3) | (p[E_KEY_G]     << 4);
  self.half_row_FB = (p[E_KEY_Q]          << 0) | (p[E_KEY_W] << 1) | (p[E_KEY_E] << 2) | (p[E_KEY_R]            << 3) | (p[E_KEY_T]     << 4);
  self.half_row_F7 = (p[E_KEY_1]          << 0) | (p[E_KEY_2] << 1) | (p[E_KEY_3] << 2) | (p[E_KEY_4]            << 3) | (p[E_KEY_5]     << 4);
  self.half_row_EF = (p[E_KEY_6]          << 4) | (p[E_KEY_7] << 3) | (p[E_KEY_8] << 2) | (p[E_KEY_9]            << 1) | (p[E_KEY_0]     << 0);
  self.half_row_DF = (p[E_KEY_Y]          << 4) | (p[E_KEY_U] << 3) | (p[E_KEY_I] << 2) | (p[E_KEY_O]            << 1) | (p[E_KEY_P]     << 0);
  self.half_row_BF = (p[E_KEY_H]          << 4) | (p[E_KEY_J] << 3) | (p[E_KEY_K] << 2) | (p[E_KEY_L]            << 1) | (p[E_KEY_ENTER] << 0);
  self.half_row_7F = (p[E_KEY_B]          << 4) | (p[E_KEY_N] << 3) | (p[E_KEY_M] << 2) | (p[E_KEY_SYMBOL_SHIFT] << 1) | (p[E_KEY_SPACE] << 0);
}


u8_t keyboard_read(u16_t address) {
  const u8_t half_row = ~(address >> 8);
  u8_t       pressed  = ~0x1F;  /* No key pressed. */

  if (half_row & ~0xFE) pressed |= self.half_row_FE;  /* ~11111110 */
  if (half_row & ~0xFD) pressed |= self.half_row_FD;  /* ~11111101 */
  if (half_row & ~0xFB) pressed |= self.half_row_FB;  /* ~11111011 */
  if (half_row & ~0xF7) pressed |= self.half_row_F7;  /* ~11110111 */
  if (half_row & ~0xEF) pressed |= self.half_row_EF;  /* ~11101111 */
  if (half_row & ~0xDF) pressed |= self.half_row_DF;  /* ~11011111 */
  if (half_row & ~0xBF) pressed |= self.half_row_BF;  /* ~10111111 */
  if (half_row & ~0x7F) pressed |= self.half_row_7F;  /* ~01111111 */

  return ~pressed;
  
}
