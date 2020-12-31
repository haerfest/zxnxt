#ifndef __ULA_H
#define __ULA_H


#include <SDL2/SDL.h>
#include "clock.h"
#include "defs.h"


typedef enum {
  E_ULA_DISPLAY_TIMING_INTERNAL_USE = 0,
  E_ULA_DISPLAY_TIMING_ZX_48K,
  E_ULA_DISPLAY_TIMING_ZX_128K,
  E_ULA_DISPLAY_TIMING_ZX_PLUS_2A,
  E_ULA_DISPLAY_TIMING_PENTAGON
} ula_display_timing_t;


typedef enum {
  E_ULA_DISPLAY_FREQUENCY_50HZ = 0,
  E_ULA_DISPLAY_FREQUENCY_60HZ
} ula_display_frequency_t;


int                  ula_init(SDL_Renderer* renderer, SDL_Texture* texture, u8_t* sram);
void                 ula_finit(void);
u8_t                 ula_read(u16_t address);
void                 ula_write(u16_t address, u8_t value);
ula_display_timing_t ula_display_timing_get(void);
void                 ula_display_timing_set(ula_display_timing_t timing);
void                 ula_display_frequency_set(ula_display_frequency_t frequency);
void                 ula_palette_set(int use_second);


#endif  /* __ULA_H */
