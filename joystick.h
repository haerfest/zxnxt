#ifndef __JOYSTICK_H
#define __JOYSTICK_H


#include <SDL2/SDL.h>
#include "defs.h"


typedef enum {
  E_JOYSTICK_LEFT = 0,
  E_JOYSTICK_RIGHT
} joystick_t;


typedef enum {
  E_JOYSTICK_TYPE_SINCLAIR_2 = 0,
  E_JOYSTICK_TYPE_KEMPSTON_1,
  E_JOYSTICK_TYPE_CURSOR,
  E_JOYSTICK_TYPE_SINCLAIR_1,
  E_JOYSTICK_TYPE_KEMPSTON_2,
  E_JOYSTICK_TYPE_MD_1,
  E_JOYSTICK_TYPE_MD_2,
  E_JOYSTICK_TYPE_USER_DEFINED_KEYS
} joystick_type_t;

  
int             joystick_init(SDL_GameController* controller_1, SDL_GameController* controller_2);
void            joystick_finit(void);
joystick_type_t joystick_type_get(joystick_t n);
void            joystick_type_set(joystick_t n, joystick_type_t type);
u8_t            joystick_kempston_read(joystick_t n);
void            joystick_refresh(void);


#endif  /* __JOYSTICK_H */
