#ifndef __KEMPSTON_H
#define __KEMPSTON_H


#include <SDL2/SDL.h>
#include "defs.h"


int  kempston_init(SDL_GameController* controller);
void kempston_finit(void);
u8_t kempston_read(u16_t address);
void kempston_refresh(void);


#endif  /* __KEMPSTON_H */
