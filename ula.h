#ifndef __ULA_H
#define __ULA_H


#include <SDL2/SDL.h>
#include "defs.h"


typedef enum {
  E_ULA_DISPLAY_TIMING_INTERNAL_USE = 0,
  E_ULA_DISPLAY_TIMING_ZX_48K,
  E_ULA_DISPLAY_TIMING_ZX_128K,
  E_ULA_DISPLAY_TIMING_ZX_PLUS_2A,
  E_ULA_DISPLAY_TIMING_PENTAGON
} ula_display_timing_t;


int                  ula_init(SDL_Renderer* renderer, SDL_Texture* texture, SDL_AudioDeviceID audio_device, u8_t* sram);
void                 ula_finit(void);
u8_t                 ula_read(u16_t address);
void                 ula_write(u16_t address, u8_t value);
ula_display_timing_t ula_display_timing_get(void);
void                 ula_display_timing_set(ula_display_timing_t timing);
void                 ula_display_frequency_set(int is_60hz);
void                 ula_palette_set(int use_second);
void                 ula_run(u32_t ticks);
void                 ula_clip_set(u8_t x1, u8_t x2, u8_t y1, u8_t y2);
void                 ula_audio_callback(void* userdata, u8_t* stream, int length);
void                 ula_audio_sync(void);


#endif  /* __ULA_H */
