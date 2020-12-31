#include <SDL2/SDL.h>
#include <stdlib.h>
#include "clock.h"
#include "defs.h"
#include "log.h"
#include "ula.h"


/**
 * See https://wiki.specnext.dev/Refresh_Rates:
 *
 * "[...] VGA 0, the perfect timings where the system clock is 28MHz. As you
 * move up through VGA 1 to VGA 6, the system clock is increased according to
 * nextreg 0x11. [...] The video mode chosen should always be VGA 0 or RGB at
 * 50Hz. This option of increasing steps VGA 1-6 are for monitors that cannot
 * sync to 50Hz only. [...] But all the relative timing relationships are
 * broken, and programs depending on timing will not display properly, just
 * like with HDMI."
 */
static const unsigned int frequency_28mhz[E_CLOCK_VIDEO_TIMING_HDMI - E_CLOCK_VIDEO_TIMING_VGA_BASE + 1] = {
  28000000,  /* E_CLOCK_VIDEO_TIMING_VGA_BASE      */
  28571429,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_1 */
  29464286,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_2 */
  30000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_3 */
  31000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_4 */
  32000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_5 */
  33000000,  /* E_CLOCK_VIDEO_TIMING_VGA_SETTING_6 */
  27000000   /* E_CLOCK_VIDEO_TIMING_HDMI          */
};


typedef struct {
  clock_video_timing_t video_timing;
  clock_cpu_speed_t    cpu_speed;
  u64_t                ticks_28mhz;   /* At max dot clock overflows in 20k years. */
  u64_t                sync_7mhz;     /* Last 28 MHz tick where we synced the 7 MHz clock. */
  u64_t                sync_reality;  /* Last 28 MHz tick where we synced with reality. */
  u64_t                time_reality;  /* Time at the last reality check. */
  u64_t                time_frequency;
  
} next_clock_t;


static next_clock_t self;


int clock_init(void) {
  self.video_timing   = E_CLOCK_VIDEO_TIMING_VGA_BASE;
  self.cpu_speed      = E_CLOCK_CPU_SPEED_3MHZ;
  self.ticks_28mhz    = 0;
  self.sync_7mhz      = self.ticks_28mhz;
  self.sync_reality   = self.ticks_28mhz;
  self.time_reality   = SDL_GetPerformanceCounter();
  self.time_frequency = SDL_GetPerformanceFrequency();

  return 0;
}


void clock_finit(void) {
}


clock_cpu_speed_t clock_cpu_speed_get(void) {
  return self.cpu_speed;
}


void clock_cpu_speed_set(clock_cpu_speed_t speed) {
  const char* speeds[] = {
    "3.5", "7", "14", "28"
  };

  self.cpu_speed = speed;
  log_dbg("clock: CPU speed set to %s MHz\n", speeds[speed]);
}


clock_video_timing_t clock_video_timing_get(void) {
  return self.video_timing;
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

  self.video_timing = timing;
  log_dbg("clock: video timing set to %s\n", descriptions[timing]);
}


void clock_run(u32_t cpu_ticks) {
  const unsigned int clock_divider[E_CLOCK_CPU_SPEED_28MHZ - E_CLOCK_CPU_SPEED_3MHZ + 1] = {
    8, 4, 2, 1
  };
  const u32_t ticks_28mhz = cpu_ticks * clock_divider[self.cpu_speed];
  u32_t       ticks_7mhz;
  u64_t       ticks_elapsed;

  /* Update system clock. */
  self.ticks_28mhz += ticks_28mhz;

  /* Update 7 MHz clock. */
  ticks_7mhz = (self.ticks_28mhz - self.sync_7mhz) / 4;
  if (ticks_7mhz > 0) {
    ula_run(ticks_7mhz);
    self.sync_7mhz += ticks_7mhz * 4;
  }

  /* Align with reality roughly once a second. */
  ticks_elapsed = self.ticks_28mhz - self.sync_reality;
  if (ticks_elapsed >= frequency_28mhz[self.video_timing]) {
    const u64_t now           = SDL_GetPerformanceCounter();
    const u64_t ticks_reality = frequency_28mhz[self.video_timing] * (now - self.time_reality) / self.time_frequency;
    const float percentage    = 100.0 * ticks_elapsed / ticks_reality;

    log_inf("clock: emulating at %.1f%% speed\n", percentage);

    self.sync_reality = self.ticks_28mhz;
    self.time_reality = now;
  }
}
