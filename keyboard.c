#include <SDL2/SDL.h>
#include "defs.h"
#include "keyboard.h"
#include "log.h"


#define N_LAYOUTS  2

typedef enum layout_t {
  E_LAYOUT_SPECTRUM,
  E_LAYOUT_HOST
} layout_t;

typedef void (*layout_handler_t)(void);


#define N_ROWS  8
#define N_COLS  5


/* All the physical keys on a ZX Spectrum 48K. */
typedef enum spectrum_key_t {
  E_KEY_1,          E_KEY_2, E_KEY_3, E_KEY_4, E_KEY_5, E_KEY_6, E_KEY_7, E_KEY_8, E_KEY_9,            E_KEY_0,
  E_KEY_Q,          E_KEY_W, E_KEY_E, E_KEY_R, E_KEY_T, E_KEY_Y, E_KEY_U, E_KEY_I, E_KEY_O,            E_KEY_P,
  E_KEY_A,          E_KEY_S, E_KEY_D, E_KEY_F, E_KEY_G, E_KEY_H, E_KEY_J, E_KEY_K, E_KEY_L,            E_KEY_ENTER,
  E_KEY_CAPS_SHIFT, E_KEY_Z, E_KEY_X, E_KEY_C, E_KEY_V, E_KEY_B, E_KEY_N, E_KEY_M, E_KEY_SYMBOL_SHIFT, E_KEY_SPACE,
} spectrum_key_t;


#define N_KEYS  (E_KEY_SPACE - E_KEY_1 + 1)


const static SDL_Scancode scancodes[N_KEYS] = {
  SDL_SCANCODE_1,      SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,    SDL_SCANCODE_0,
  SDL_SCANCODE_Q,      SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O,    SDL_SCANCODE_P,
  SDL_SCANCODE_A,      SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,    SDL_SCANCODE_RETURN,
  SDL_SCANCODE_LSHIFT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE
};


typedef struct self_t {
  const u8_t*      state;
  layout_t         layout;
  layout_handler_t layout_handler;
  int              pressed[N_KEYS];
} self_t;


static self_t self;


static void layout_handler_spectrum(void);
static void layout_handler_host(void);


int keyboard_init(void) {
  self.layout         = E_LAYOUT_SPECTRUM;
  self.layout_handler = layout_handler_spectrum;
  self.state          = SDL_GetKeyboardState(NULL);
  return 0;
}


void keyboard_finit(void) {
}


void keyboard_toggle_layout(void) {
  self.layout         = E_LAYOUT_HOST - self.layout;
  self.layout_handler = (self.layout == E_LAYOUT_SPECTRUM) ? layout_handler_spectrum : layout_handler_host;
}


int keyboard_is_special_key_pressed(keyboard_special_key_t key) {
  switch (key) {
    case E_KEYBOARD_SPECIAL_KEY_CPU_SPEED:
      return self.state[SDL_SCANCODE_F8] ? 1 : 0;

    case E_KEYBOARD_SPECIAL_KEY_DRIVE:
      return self.state[SDL_SCANCODE_F10] ? 1 : 0;
      
    case E_KEYBOARD_SPECIAL_KEY_NMI:
      return self.state[SDL_SCANCODE_F9] ? 1 : 0;
      
    case E_KEYBOARD_SPECIAL_KEY_RESET_HARD:
      return self.state[SDL_SCANCODE_F1] ? 1 : 0;

    case E_KEYBOARD_SPECIAL_KEY_RESET_SOFT:
      return self.state[SDL_SCANCODE_F4] ? 1 : 0;

    default:
      return 0;
  }
} 


#define HALF_ROW_L(a,b,c,d,e) ((self.pressed[a] << 0) | (self.pressed[b] << 1) | (self.pressed[c] << 2) | (self.pressed[d] << 3) | self.pressed[e] << 4)
#define HALF_ROW_R(a,b,c,d,e) ((self.pressed[a] << 4) | (self.pressed[b] << 3) | (self.pressed[c] << 2) | (self.pressed[d] << 1) | self.pressed[e] << 0)


/* TODO: Use Caps Lock to trigger Caps Shift + 2. */
static void layout_handler_spectrum(void) {
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
    /* Delete. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_0] = 1;
  }
  if (self.state[SDL_SCANCODE_GRAVE]) {
    /* Edit. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_1] = 1;
  }
  if (self.state[SDL_SCANCODE_LEFT]) {
    /* Left. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_5] = 1;
  }
  if (self.state[SDL_SCANCODE_DOWN]) {
    /* Down. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_6] = 1;
  }
  if (self.state[SDL_SCANCODE_UP]) {
    /* Up. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_7] = 1;
  }
  if (self.state[SDL_SCANCODE_RIGHT]) {
    /* Right. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_8] = 1;
  }
  if (self.state[SDL_SCANCODE_ESCAPE]) {
    /* Break. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_SPACE] = 1;
  }
  if (self.state[SDL_SCANCODE_SEMICOLON]) {
    /* ; */
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_O] = 1;
  }
  if (self.state[SDL_SCANCODE_APOSTROPHE]) {
    /* " */
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_P] = 1;
  }
  if (self.state[SDL_SCANCODE_COMMA]) {
    /* , */
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_N] = 1;
  }
  if (self.state[SDL_SCANCODE_PERIOD]) {
    /* . */
    self.pressed[E_KEY_SYMBOL_SHIFT] = self.pressed[E_KEY_M] = 1;
  }
  if (self.state[SDL_SCANCODE_TAB]) {
    /* Extend Mode. */
    self.pressed[E_KEY_CAPS_SHIFT] = self.pressed[E_KEY_SYMBOL_SHIFT] = 1;
  }
}


static void layout_handler_host(void) {
  /*
  E_KEY_1,          E_KEY_2, E_KEY_3, E_KEY_4, E_KEY_5, E_KEY_6, E_KEY_7, E_KEY_8, E_KEY_9,            E_KEY_0,
  E_KEY_Q,          E_KEY_W, E_KEY_E, E_KEY_R, E_KEY_T, E_KEY_Y, E_KEY_U, E_KEY_I, E_KEY_O,            E_KEY_P,
  E_KEY_A,          E_KEY_S, E_KEY_D, E_KEY_F, E_KEY_G, E_KEY_H, E_KEY_J, E_KEY_K, E_KEY_L,            E_KEY_ENTER,
  E_KEY_CAPS_SHIFT, E_KEY_Z, E_KEY_X, E_KEY_C, E_KEY_V, E_KEY_B, E_KEY_N, E_KEY_M, E_KEY_SYMBOL_SHIFT, E_KEY_SPACE,

  SIN           SIN  Extended (Symbol Shift + Caps Shift, [Win/Alt] + [Shift], or [Tab])
  Q  <=         q    Normal (L)     Q Caps Shift (L)    PLOT  Normal (K)
   PLOT         <=   Symbol Shift [Win/Alt]
  ASN           ASN  Extended then sole Symbol Shift or Caps Shift
*/

  const u8_t shift = self.state[SDL_SCANCODE_LSHIFT] | self.state[SDL_SCANCODE_RSHIFT];
  const u8_t alt   = self.state[SDL_SCANCODE_LALT]   | self.state[SDL_SCANCODE_RALT];

  /* 1 2 3 4 5 6 7 8 9 0 */
  self.pressed[E_KEY_1] = self.state[SDL_SCANCODE_1]
                        | (self.state[SDL_SCANCODE_GRAVE] & !shift);  /* Edit */
  self.pressed[E_KEY_2] = self.state[SDL_SCANCODE_2]
                        | self.state[SDL_SCANCODE_CAPSLOCK];
  self.pressed[E_KEY_3] = self.state[SDL_SCANCODE_3];
  self.pressed[E_KEY_4] = self.state[SDL_SCANCODE_4];
  self.pressed[E_KEY_5] = self.state[SDL_SCANCODE_5]
                        | ((!shift) & self.state[SDL_SCANCODE_LEFT]);
  self.pressed[E_KEY_6] = ((!shift) & ( self.state[SDL_SCANCODE_6]
                                      | self.state[SDL_SCANCODE_DOWN]))
                        | (shift & self.state[SDL_SCANCODE_7]);  /* & */
  self.pressed[E_KEY_7] = (!shift) & ( self.state[SDL_SCANCODE_7]
                                     | self.state[SDL_SCANCODE_UP]
                                     | self.state[SDL_SCANCODE_APOSTROPHE]);
  self.pressed[E_KEY_8] = ((!shift) & ( self.state[SDL_SCANCODE_8]
                                      | self.state[SDL_SCANCODE_RIGHT]))
                        | (shift & self.state[SDL_SCANCODE_9]);       /* ( */
  self.pressed[E_KEY_9] = ((!shift) & self.state[SDL_SCANCODE_9])
                        | (shift & self.state[SDL_SCANCODE_0]);       /* ) */
  self.pressed[E_KEY_0] = ((!shift) & ( self.state[SDL_SCANCODE_0]
                                      | self.state[SDL_SCANCODE_BACKSPACE]))
                        | (shift & self.state[SDL_SCANCODE_MINUS]);  /* _ */

  /* Q W E R T Y U I O P */
  self.pressed[E_KEY_Q] = self.state[SDL_SCANCODE_Q];
  self.pressed[E_KEY_W] = self.state[SDL_SCANCODE_W];
  self.pressed[E_KEY_E] = self.state[SDL_SCANCODE_E];
  self.pressed[E_KEY_R] = self.state[SDL_SCANCODE_R]
                        | (shift & self.state[SDL_SCANCODE_COMMA]);            /* < */
  self.pressed[E_KEY_T] = self.state[SDL_SCANCODE_T]
                        | (shift & self.state[SDL_SCANCODE_PERIOD]);           /* > */
  self.pressed[E_KEY_Y] = self.state[SDL_SCANCODE_Y]
                        | ((!shift) & self.state[SDL_SCANCODE_LEFTBRACKET]);   /* [ */
  self.pressed[E_KEY_U] = self.state[SDL_SCANCODE_U]
                        | ((!shift) & self.state[SDL_SCANCODE_RIGHTBRACKET]);  /* ] */
  self.pressed[E_KEY_I] = self.state[SDL_SCANCODE_I];
  self.pressed[E_KEY_O] = self.state[SDL_SCANCODE_O]
                        | ((!shift) & self.state[SDL_SCANCODE_SEMICOLON]);     /* ; */
  self.pressed[E_KEY_P] = self.state[SDL_SCANCODE_P]
                        | (shift & self.state[SDL_SCANCODE_APOSTROPHE]);       /* " */

  /* A S D F G H J K L ENTER */
  self.pressed[E_KEY_A] = self.state[SDL_SCANCODE_A]
                        | (shift & ( self.state[SDL_SCANCODE_GRAVE]         /* ~ */
                                   | self.state[SDL_SCANCODE_NONUSBACKSLASH]));
  self.pressed[E_KEY_S] = self.state[SDL_SCANCODE_S]
                        | (shift & self.state[SDL_SCANCODE_BACKSLASH]);     /* | */
  self.pressed[E_KEY_D] = self.state[SDL_SCANCODE_D]
                        | ((!shift) & self.state[SDL_SCANCODE_BACKSLASH]);  /* \ */
  self.pressed[E_KEY_F] = self.state[SDL_SCANCODE_F]
                        | (shift & self.state[SDL_SCANCODE_LEFTBRACKET]);   /* { */
  self.pressed[E_KEY_G] = self.state[SDL_SCANCODE_G]
                        | (shift & self.state[SDL_SCANCODE_RIGHTBRACKET]);  /* } */
  self.pressed[E_KEY_H] = self.state[SDL_SCANCODE_H]
                        | (shift & self.state[SDL_SCANCODE_6]);             /* ^ */
  self.pressed[E_KEY_J] = self.state[SDL_SCANCODE_J]
                        | ((!shift) & self.state[SDL_SCANCODE_MINUS]);      /* - */
  self.pressed[E_KEY_K] = self.state[SDL_SCANCODE_K]
                        | (shift & self.state[SDL_SCANCODE_EQUALS]);        /* + */
  self.pressed[E_KEY_L] = self.state[SDL_SCANCODE_L]
                        | ((!shift) & self.state[SDL_SCANCODE_EQUALS]);     /* = */
  self.pressed[E_KEY_ENTER] = self.state[SDL_SCANCODE_RETURN];

  /* CAPS-SHIFT Z X C V B N M SYMBOL-SHIFT SPACE */
  self.pressed[E_KEY_CAPS_SHIFT] = self.state[SDL_SCANCODE_TAB]            /* Extended */
                                 | self.state[SDL_SCANCODE_CAPSLOCK]
                                 | self.state[SDL_SCANCODE_BACKSPACE]
                                 | self.state[SDL_SCANCODE_ESCAPE]
                                 | self.state[SDL_SCANCODE_LEFT]
                                 | self.state[SDL_SCANCODE_RIGHT]
                                 | self.state[SDL_SCANCODE_UP]
                                 | self.state[SDL_SCANCODE_DOWN]
                                 | (shift & ( self.state[SDL_SCANCODE_Q]
                                            | self.state[SDL_SCANCODE_W]
                                            | self.state[SDL_SCANCODE_E]
                                            | self.state[SDL_SCANCODE_R]
                                            | self.state[SDL_SCANCODE_T]
                                            | self.state[SDL_SCANCODE_Y]
                                            | self.state[SDL_SCANCODE_U]
                                            | self.state[SDL_SCANCODE_I]
                                            | self.state[SDL_SCANCODE_O]
                                            | self.state[SDL_SCANCODE_P]
                                            | self.state[SDL_SCANCODE_A]
                                            | self.state[SDL_SCANCODE_S]
                                            | self.state[SDL_SCANCODE_D]
                                            | self.state[SDL_SCANCODE_F]
                                            | self.state[SDL_SCANCODE_G]
                                            | self.state[SDL_SCANCODE_H]
                                            | self.state[SDL_SCANCODE_J]
                                            | self.state[SDL_SCANCODE_K]
                                            | self.state[SDL_SCANCODE_L]
                                            | self.state[SDL_SCANCODE_Z]
                                            | self.state[SDL_SCANCODE_X]
                                            | self.state[SDL_SCANCODE_C]
                                            | self.state[SDL_SCANCODE_V]
                                            | self.state[SDL_SCANCODE_B]
                                            | self.state[SDL_SCANCODE_N]
                                            | self.state[SDL_SCANCODE_M]));
  self.pressed[E_KEY_Z] = self.state[SDL_SCANCODE_Z]
                        | (shift & self.state[SDL_SCANCODE_SEMICOLON]);     /* : */
  self.pressed[E_KEY_X] = self.state[SDL_SCANCODE_X]
                        | ((!shift) & alt & self.state[SDL_SCANCODE_2]);    /* pound */
  self.pressed[E_KEY_C] = self.state[SDL_SCANCODE_C]
                        | (shift & self.state[SDL_SCANCODE_SLASH]);         /* ? */
  self.pressed[E_KEY_V] = self.state[SDL_SCANCODE_V]
                          | ((!shift) & self.state[SDL_SCANCODE_SLASH]);    /* / */
  self.pressed[E_KEY_B] = self.state[SDL_SCANCODE_B]
                        | (shift & self.state[SDL_SCANCODE_8]);             /* * */
  self.pressed[E_KEY_N] = self.state[SDL_SCANCODE_N]
                        | ((!shift) & self.state[SDL_SCANCODE_COMMA]);      /* , */
  self.pressed[E_KEY_M] = self.state[SDL_SCANCODE_M]
                        | ((!shift) & self.state[SDL_SCANCODE_PERIOD]);     /* . */
  self.pressed[E_KEY_SYMBOL_SHIFT] = alt
                                   | self.state[SDL_SCANCODE_APOSTROPHE]
                                   | self.state[SDL_SCANCODE_SEMICOLON]
                                   | self.state[SDL_SCANCODE_MINUS]
                                   | self.state[SDL_SCANCODE_EQUALS]
                                   | self.state[SDL_SCANCODE_SLASH]
                                   | self.state[SDL_SCANCODE_COMMA]
                                   | self.state[SDL_SCANCODE_PERIOD]
                                   | self.state[SDL_SCANCODE_LEFTBRACKET]
                                   | self.state[SDL_SCANCODE_RIGHTBRACKET]
                                   | self.state[SDL_SCANCODE_BACKSLASH]
                                   | self.state[SDL_SCANCODE_TAB]  /* Extended. */
                                   | (shift & ( self.state[SDL_SCANCODE_1]
                                              | self.state[SDL_SCANCODE_2]
                                              | self.state[SDL_SCANCODE_3]
                                              | self.state[SDL_SCANCODE_4]
                                              | self.state[SDL_SCANCODE_5]
                                              | self.state[SDL_SCANCODE_6]
                                              | self.state[SDL_SCANCODE_7]
                                              | self.state[SDL_SCANCODE_8]
                                              | self.state[SDL_SCANCODE_9]
                                              | self.state[SDL_SCANCODE_0]
                                              | self.state[SDL_SCANCODE_GRAVE]
                                              | self.state[SDL_SCANCODE_NONUSBACKSLASH]));
  self.pressed[E_KEY_SPACE] = self.state[SDL_SCANCODE_SPACE]
                            | ((!shift) & self.state[SDL_SCANCODE_ESCAPE]);  /* break */
}


/**
 *      Left half row    Right half row
 * ------------------    ------------------------
 *          1 2 3 4 5    6 7 8 9            0
 *          Q W E R T    Y U I O            P
 *          A S D F G    H J K L            ENTER
 * CAPS_SHIFT Z X C V    B N M SYMBOL_SHIFT SPACE
 */
u8_t keyboard_read(u16_t address) {
  const u8_t half_row = ~(address >> 8);
  u8_t       pressed  = ~0x1F;  /* No key pressed. */

  self.layout_handler();

  if (half_row & ~0xFE) pressed |= HALF_ROW_L(E_KEY_CAPS_SHIFT, E_KEY_Z, E_KEY_X, E_KEY_C,            E_KEY_V    );  /* ~11111110 */
  if (half_row & ~0xFD) pressed |= HALF_ROW_L(E_KEY_A,          E_KEY_S, E_KEY_D, E_KEY_F,            E_KEY_G    );  /* ~11111101 */
  if (half_row & ~0xFB) pressed |= HALF_ROW_L(E_KEY_Q,          E_KEY_W, E_KEY_E, E_KEY_R,            E_KEY_T    );  /* ~11111011 */
  if (half_row & ~0xF7) pressed |= HALF_ROW_L(E_KEY_1,          E_KEY_2, E_KEY_3, E_KEY_4,            E_KEY_5    );  /* ~11110111 */
  if (half_row & ~0xEF) pressed |= HALF_ROW_R(E_KEY_6,          E_KEY_7, E_KEY_8, E_KEY_9,            E_KEY_0    );  /* ~11101111 */
  if (half_row & ~0xDF) pressed |= HALF_ROW_R(E_KEY_Y,          E_KEY_U, E_KEY_I, E_KEY_O,            E_KEY_P    );  /* ~11011111 */
  if (half_row & ~0xBF) pressed |= HALF_ROW_R(E_KEY_H,          E_KEY_J, E_KEY_K, E_KEY_L,            E_KEY_ENTER);  /* ~10111111 */
  if (half_row & ~0x7F) pressed |= HALF_ROW_R(E_KEY_B,          E_KEY_N, E_KEY_M, E_KEY_SYMBOL_SHIFT, E_KEY_SPACE);  /* ~01111111 */

  return ~pressed;
}
