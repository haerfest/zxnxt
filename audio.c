#include <SDL2/SDL.h>
#include "audio.h"
#include "clock.h"
#include "defs.h"


#define N_SOURCES  (E_AUDIO_SOURCE_AY_1_CHANNEL_C - E_AUDIO_SOURCE_BEEPER + 1)


typedef struct {
  SDL_AudioDeviceID device;
  s8_t              last_sample[N_SOURCES];
  s8_t              mixed[AUDIO_BUFFER_LENGTH];
  s16_t             mixed_last_sample_sum;
  s8_t              mixed_last_sample;
  u64_t             emptied_ticks_28mhz;
  SDL_bool          is_empty;
  SDL_cond*         emptied;
  SDL_mutex*        lock;
} self_t;


static self_t self;


int audio_init(SDL_AudioDeviceID device) {
  self.device                = device;
  self.emptied_ticks_28mhz   = clock_ticks();
  self.is_empty              = SDL_FALSE;
  self.emptied               = SDL_CreateCond();
  self.lock                  = SDL_CreateMutex();
  self.mixed_last_sample_sum = 0;
  self.mixed_last_sample     = 0;

  memset(self.last_sample, 0, sizeof(self.last_sample));
  memset(self.mixed,       0, sizeof(self.mixed));

  return 0;
}


void audio_finit(void) {
}


void audio_add_sample(audio_source_t source, s8_t sample) {
  size_t index;

  /* Prevent a race condition between us and the audio callback. */
  SDL_LockAudioDevice(self.device);

  /* Calculate where to place the new sample. */
  index = AUDIO_SAMPLE_RATE * (clock_ticks() - self.emptied_ticks_28mhz) / 28000000;

  if (index < AUDIO_BUFFER_LENGTH) {
    /* Unmix my previous sample, and mix in the new one. */
    self.mixed_last_sample_sum -= self.last_sample[source];
    self.mixed_last_sample_sum += sample;
    self.mixed_last_sample      = self.mixed_last_sample_sum / N_SOURCES;
    
    /* Place this sample and extend it to the end of the buffer, assuming
     * for now that no further samples will arrive before the callback
     * picks up our audio buffer. */
    memset(&self.mixed[index], self.mixed_last_sample, sizeof(self.mixed) - index);
  }

  self.last_sample[source] = sample;

  SDL_UnlockAudioDevice(self.device);
}


void audio_resume(void) {
  SDL_PauseAudioDevice(self.device, 0);
}


void audio_pause(void) {
  SDL_PauseAudioDevice(self.device, 1);
}


void audio_sync(void) {
  SDL_LockMutex(self.lock);

  while (!self.is_empty) {
    SDL_CondWait(self.emptied, self.lock);
  }
  self.is_empty = SDL_FALSE;

  SDL_UnlockMutex(self.lock);
}


void audio_callback(void* userdata, u8_t* stream, int length) {
  /* Copy audio mix to SDL stream. */
  memcpy(stream, self.mixed, sizeof(self.mixed));

  /* Reset to silence. */
  memset(self.mixed, self.mixed_last_sample, sizeof(self.mixed));

  /* Record when we last emptied the buffer, so we can know where in the buffer
   * to place freshly arriving audio samples. */
  self.emptied_ticks_28mhz = clock_ticks();

  /* Signal the main thread that we emptied the audio buffer. */
  SDL_LockMutex(self.lock);
  self.is_empty = SDL_TRUE;
  SDL_CondSignal(self.emptied);
  SDL_UnlockMutex(self.lock);
}
