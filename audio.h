#ifndef __AUDIO_H
#define __AUDIO_H


#include <SDL2/SDL.h>
#include "defs.h"


#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_BUFFER_LENGTH   2048


int  audio_init(SDL_AudioDeviceID device);
void audio_finit(void);
void audio_pause(void);
void audio_resume(void);
void audio_add_sample(s8_t sample);
void audio_sync(void);
void audio_callback(void* userdata, u8_t* stream, int length);

#endif  /* __AUDIO_H */
