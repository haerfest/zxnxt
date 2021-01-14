#ifndef __AUDIO_H
#define __AUDIO_H


#include <SDL2/SDL.h>
#include "defs.h"


typedef enum {
  E_AUDIO_SOURCE_BEEPER,
  E_AUDIO_SOURCE_AY_1_CHANNEL_A,
  E_AUDIO_SOURCE_AY_1_CHANNEL_B,
  E_AUDIO_SOURCE_AY_1_CHANNEL_C,
  E_AUDIO_SOURCE_AY_2_CHANNEL_A,
  E_AUDIO_SOURCE_AY_2_CHANNEL_B,
  E_AUDIO_SOURCE_AY_2_CHANNEL_C,
  E_AUDIO_SOURCE_AY_3_CHANNEL_A,
  E_AUDIO_SOURCE_AY_3_CHANNEL_B,
  E_AUDIO_SOURCE_AY_3_CHANNEL_C
} audio_source_t;


typedef enum {
  E_AUDIO_CHANNEL_LEFT,
  E_AUDIO_CHANNEL_RIGHT,
  E_AUDIO_CHANNEL_BOTH
} audio_channel_t;


#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_BUFFER_LENGTH    512
#define AUDIO_N_CHANNELS         2
#define AUDIO_MAX_VOLUME       127


int  audio_init(SDL_AudioDeviceID device);
void audio_finit(void);
void audio_pause(void);
void audio_resume(void);
void audio_assign_channel(audio_source_t source, audio_channel_t channel);
void audio_add_sample(audio_source_t source, s8_t sample);
void audio_sync(void);
void audio_callback(void* userdata, u8_t* stream, int length);


#endif  /* __AUDIO_H */
