#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "clock.h"


static const unsigned int clock_frequency_28mhz[E_CLOCK_VIDEO_TIMING_HDMI - E_CLOCK_VIDEO_TIMING_VGA_BASE + 1] = {
  28000000,  /* E_CLOCK_VIDEO_TIMING_VGA_BASE      */
  28571429,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_1 */
  29464286,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_2 */
  30000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_3 */
  31000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_4 */
  32000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_5 */
  33000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_6 */
  27000000   /* E_CLOCK_VIDEO_TIMING_HDMI          */
};


static const unsigned int clock_divider[E_CLOCK_CPU_SPEED_28MHZ - E_CLOCK_CPU_SPEED_3MHZ + 1] = {
  8,
  4,
  2,
  1
};


typedef struct {
  clock_video_timing_t   video_timing;
  clock_display_timing_t display_timing;
  clock_cpu_speed_t      cpu_speed;
  u64_t                  ticks;  /* At max dot clock overflows in 20k years. */
  unsigned int           n_callbacks;
  clock_callback_t*      callbacks;
} clock_t;


static clock_t clock;


int clock_init(void) {
  clock.video_timing   = E_CLOCK_VIDEO_TIMING_VGA_BASE;
  clock.display_timing = E_CLOCK_DISPLAY_TIMING_ZX_PLUS_2A;
  clock.cpu_speed      = E_CLOCK_CPU_SPEED_3MHZ;
  clock.ticks          = 0;
  clock.n_callbacks    = 0;
  clock.callbacks      = NULL;

  return 0;
}


void clock_finit(void) {
}


void clock_cpu_speed_set(clock_cpu_speed_t speed) {
  const char* speeds[] = {
    "3.5", "7", "14", "28"
  };

  clock.cpu_speed = speed;
  fprintf(stderr, "clock: CPU speed set to %s MHz\n", speeds[speed]);
}


clock_display_timing_t clock_display_timing_get(void) {
  return clock.display_timing;
}


void clock_display_timing_set(clock_display_timing_t timing) {
  const char* descriptions[] = {
    "internal use",
    "ZX Spectrum 48K",
    "ZX Spectrum 128K/+2",
    "ZX Spectrum +2A/+2B/+3",
    "Pentagon",
    "invalid (5)",
    "invalid (6)",
    "invalid (7)"
  };

  clock.display_timing = timing;
  fprintf(stderr, "clock: display timing set to %s\n", descriptions[timing]);
}


clock_video_timing_t clock_video_timing_get(void) {
  return clock.video_timing;
}


void clock_video_timing_set(clock_video_timing_t timing) {
  const char* descriptions[] = {
    "base VGA",
    "VGA setting 1",
    "VGA setting 2",
    "VGA setting 3",
    "VGA setting 4",
    "VGA setting 5",
    "VGA setting 6",
    "HDMI"
  };

  clock.video_timing = timing;
  fprintf(stderr, "clock: video timing set to %s\n", descriptions[timing]);
}


int clock_register_callback(clock_callback_t callback) {
  clock_callback_t* callbacks = realloc(clock.callbacks, (clock.n_callbacks + 1) * sizeof(clock_callback_t));
  if (callbacks == NULL) {
    fprintf(stderr, "clock: out of memory\n");
    return -1;
  }

  clock.callbacks = callbacks;
  clock.callbacks[clock.n_callbacks] = callback;
  clock.n_callbacks++;

  return 0;
}


static void clock_invoke_callbacks(u64_t ticks, unsigned int delta) {
  unsigned int i;
  
  for (i = 0; i < clock.n_callbacks; i++) {
    (*clock.callbacks[i])(ticks, delta);
  }
}


/* Called by the CPU, telling us how many of its ticks have passed. */
void clock_ticks(unsigned int cpu_ticks) {
  const unsigned int clock_frequency = clock_frequency_28mhz[clock.video_timing];
  const unsigned     ticks_28mhz     = cpu_ticks * clock_divider[clock.cpu_speed];

  clock.ticks += ticks_28mhz;
  clock_invoke_callbacks(clock.ticks, ticks_28mhz);

  /**
   * Fastest ~28 MHz clock runs at 33 MHz.
   * That is 33,000,000 ticks per second.
   * If we want to adjust emulated speed 10 times per second,
   * we have to adjust every 3,300,000 ticks.
   * If we run at ~3 MHz that is every 412,500 ticks.
   * We need to measure how much real time has elapsed.
   * Ideal that should be 100 milliseconds.
   * If it's more, we are running too slow.
   * If it's less, we are running too fast.
   */
}
