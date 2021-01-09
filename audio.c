#include <SDL2/SDL.h>
#include "audio.h"
#include "clock.h"
#include "defs.h"


typedef struct {
  SDL_AudioDeviceID device;
  s8_t              buffer[AUDIO_BUFFER_LENGTH];
  u64_t             buffer_emptied_ticks_28mhz;
  s8_t              last_sample;
  SDL_bool          is_empty;
  SDL_cond*         emptied;
  SDL_mutex*        lock;
} self_t;


static self_t self;


int audio_init(SDL_AudioDeviceID device) {
  self.device                     = device;
  self.buffer_emptied_ticks_28mhz = clock_ticks();
  self.last_sample                = 0;
  self.is_empty                   = SDL_FALSE;
  self.emptied                    = SDL_CreateCond();
  self.lock                       = SDL_CreateMutex();

  memset(self.buffer, self.last_sample, sizeof(self.buffer));

  return 0;
}


void audio_finit(void) {
}


void audio_add_sample(audio_source_t source, s8_t sample) {
  size_t index;

  /* Prevent a race condition between us and the audio callback. */
  SDL_LockAudioDevice(self.device);

  /* Calculate where to place the new sample. */
  index = AUDIO_SAMPLE_RATE * (clock_ticks() - self.buffer_emptied_ticks_28mhz) / 28000000;

  if (index < sizeof(self.buffer)) {
    /* Place this sample and extend it to the end of the buffer, assuming
     * for now that no further samples will arrive before the callback
     * picks up our audio buffer. */
    memset(&self.buffer[index], sample, sizeof(self.buffer) - index);
  }

  self.last_sample = sample;

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
  /* Copy the audio buffer to SDL. */
  memcpy(stream, self.buffer, sizeof(self.buffer));

  /* Fill our buffer with silence according to the last sample,
   * in case no further samples follow on time. */
  memset(self.buffer, self.last_sample, sizeof(self.buffer));

  /* Record when we last emptied the buffer, so we can know where in the buffer
   * to place freshly arriving audio samples. */
  self.buffer_emptied_ticks_28mhz = clock_ticks();

  /* Signal the main thread that we emptied the audio buffer. */
  SDL_LockMutex(self.lock);
  self.is_empty = SDL_TRUE;
  SDL_CondSignal(self.emptied);
  SDL_UnlockMutex(self.lock);
}
