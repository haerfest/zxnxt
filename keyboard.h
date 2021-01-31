#ifndef __KEYBOARD_H
#define __KEYBOARD_H


#include "defs.h"


typedef enum {
  E_KEYBOARD_SPECIAL_KEY_CPU_SPEED,
  E_KEYBOARD_SPECIAL_KEY_DRIVE,
  E_KEYBOARD_SPECIAL_KEY_NMI,
  E_KEYBOARD_SPECIAL_KEY_REFRESH_RATE,
  E_KEYBOARD_SPECIAL_KEY_RESET_HARD,
  E_KEYBOARD_SPECIAL_KEY_RESET_SOFT,
  E_KEYBOARD_SPECIAL_KEY_SCANDOUBLER
} keyboard_special_key_t;


int  keyboard_init(void);
void keyboard_finit();
u8_t keyboard_read(u16_t address);
void keyboard_refresh(void);
int  keyboard_is_special_key_pressed(keyboard_special_key_t key);


#endif  /* __KEYBOARD_H */
