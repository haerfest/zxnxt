#include <SDL2/SDL.h>
#include "audio.h"
#include "clock.h"
#include "defs.h"


#define N_SOURCES       (E_AUDIO_SOURCE_LAST - E_AUDIO_SOURCE_FIRST + 1)
#define SQRT_N_SOURCES  4  /* More or less, provides headroom. */


typedef struct {
  SDL_AudioDeviceID device;
  audio_channel_t   channels[N_SOURCES];
  s8_t              last_sample[N_SOURCES];
  s8_t              mixed[AUDIO_BUFFER_LENGTH * AUDIO_N_CHANNELS];
  const s8_t*       mixed_end;
  s16_t             mixed_last_sample_sum_left;
  s16_t             mixed_last_sample_sum_right;
  s8_t              mixed_last_sample_left;
  s8_t              mixed_last_sample_right;
  u64_t             emptied_ticks_28mhz;
  SDL_bool          is_empty;
  SDL_cond*         emptied;
  SDL_mutex*        lock;
  u32_t             clock_28mhz;
} self_t;


static self_t self;


int audio_init(SDL_AudioDeviceID device) {
  memset(&self, 0, sizeof(self));

  self.device                = device;
  self.emptied_ticks_28mhz   = clock_ticks();
  self.mixed_end             = &self.mixed[AUDIO_BUFFER_LENGTH * AUDIO_N_CHANNELS];
  self.is_empty              = SDL_FALSE;
  self.emptied               = SDL_CreateCond();
  self.lock                  = SDL_CreateMutex();
  self.clock_28mhz           = clock_28mhz_get();

  self.channels[E_AUDIO_SOURCE_BEEPER        ] = E_AUDIO_CHANNEL_BOTH;
  self.channels[E_AUDIO_SOURCE_AY_1_CHANNEL_A] = E_AUDIO_CHANNEL_LEFT;
  self.channels[E_AUDIO_SOURCE_AY_1_CHANNEL_B] = E_AUDIO_CHANNEL_BOTH;
  self.channels[E_AUDIO_SOURCE_AY_1_CHANNEL_C] = E_AUDIO_CHANNEL_RIGHT;
  self.channels[E_AUDIO_SOURCE_DAC_A         ] = E_AUDIO_CHANNEL_LEFT;
  self.channels[E_AUDIO_SOURCE_DAC_B         ] = E_AUDIO_CHANNEL_LEFT;
  self.channels[E_AUDIO_SOURCE_DAC_C         ] = E_AUDIO_CHANNEL_RIGHT;
  self.channels[E_AUDIO_SOURCE_DAC_D         ] = E_AUDIO_CHANNEL_RIGHT;

  return 0;
}


void audio_finit(void) {
}


void audio_assign_channel(audio_source_t source, audio_channel_t channel) {
  self.channels[source] = channel;
}


void audio_add_sample(audio_source_t source, s8_t sample) {
  size_t index;

  /* Prevent a race condition between us and the audio callback. */
  SDL_LockAudioDevice(self.device);

  /* Calculate where to place the new sample. */
  index = AUDIO_SAMPLE_RATE * (clock_ticks() - self.emptied_ticks_28mhz) / self.clock_28mhz;

  if (index < AUDIO_BUFFER_LENGTH) {
    s8_t* mixed;

    /* Unmix my previous sample, and mix in the new one, per channel. */
    if (self.channels[source] != E_AUDIO_CHANNEL_RIGHT) {
      /* Left or both. */
      self.mixed_last_sample_sum_left -= self.last_sample[source];
      self.mixed_last_sample_sum_left += sample;
      self.mixed_last_sample_left      = self.mixed_last_sample_sum_left / SQRT_N_SOURCES;
    }
    if (self.channels[source] != E_AUDIO_CHANNEL_LEFT) {
      /* Right or both. */
      self.mixed_last_sample_sum_right -= self.last_sample[source];
      self.mixed_last_sample_sum_right += sample;
      self.mixed_last_sample_right      = self.mixed_last_sample_sum_right / SQRT_N_SOURCES;
    }
    
    /* Place this sample and extend it to the end of the buffer, assuming
     * for now that no further samples will arrive before the callback
     * picks up our audio buffer. */
    mixed = &self.mixed[index * AUDIO_N_CHANNELS];
    while (mixed < self.mixed_end) {
      *mixed++ = self.mixed_last_sample_left;
      *mixed++ = self.mixed_last_sample_right;
    }

    self.last_sample[source] = sample;
  }

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
  s8_t* mixed = self.mixed;

  /* Copy audio mix to SDL stream. */
  memcpy(stream, self.mixed, sizeof(self.mixed));

  /* Reset to silence. */
  while (mixed < self.mixed_end) {
    *mixed++ = self.mixed_last_sample_left;
    *mixed++ = self.mixed_last_sample_right;
  }

  /* Record when we last emptied the buffer, so we can know where in the buffer
   * to place freshly arriving audio samples. */
  self.emptied_ticks_28mhz = clock_ticks();

  /* Signal the main thread that we emptied the audio buffer. */
  SDL_LockMutex(self.lock);
  self.is_empty = SDL_TRUE;
  SDL_CondSignal(self.emptied);
  SDL_UnlockMutex(self.lock);
}


void audio_clock_28mhz_set(u32_t freq_28mhz) {
  self.clock_28mhz = freq_28mhz;
}
