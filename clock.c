#include <stdlib.h>
#include "ay.h"
#include "clock.h"
#include "defs.h"
#include "log.h"
#include "main.h"
#include "slu.h"


/**
 * https://wiki.specnext.dev/Refresh_Rates
 *
 * "[...] VGA 0, the perfect timings where the system clock is 28MHz. As you
 * move up through VGA 1 to VGA 6, the system clock is increased according to
 * nextreg 0x11. [...] The video mode chosen should always be VGA 0 or RGB at
 * 50Hz. This option of increasing steps VGA 1-6 are for monitors that cannot
 * sync to 50Hz only. [...] But all the relative timing relationships are
 * broken, and programs depending on timing will not display properly, just
 * like with HDMI."
 */
static const u32_t frequency_28mhz[E_CLOCK_VIDEO_TIMING_HDMI - E_CLOCK_VIDEO_TIMING_VGA_BASE + 1] = {
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
  u64_t                ticks_28mhz;      /* At max dot clock overflows in 20k years. */
  u64_t                sync_14mhz;       /* Last 28 MHz tick where we synced the 14 MHz ULA clock. */
  u64_t                sync_2mhz;        /* Last 28 MHz tick where we synced the 1.75 MHz AY-3-8912 clock. */
  u64_t                sync_host_next;   /* Next moment at which to sync with reality. */
} self_t;


static self_t self;


u32_t clock_28mhz_get(void) {
  return frequency_28mhz[self.video_timing];
}


int clock_init(void) {
  self.video_timing   = E_CLOCK_VIDEO_TIMING_VGA_BASE;
  self.cpu_speed      = E_CLOCK_CPU_SPEED_3MHZ;
  self.ticks_28mhz    = 0;
  self.sync_14mhz     = self.ticks_28mhz;
  self.sync_2mhz      = self.ticks_28mhz;
  self.sync_host_next = self.ticks_28mhz + main_next_host_sync_get();

  return 0;
}


void clock_finit(void) {
}


u64_t clock_ticks(void) {
  return self.ticks_28mhz;
}


clock_cpu_speed_t clock_cpu_speed_get(void) {
  return self.cpu_speed;
}


void clock_cpu_speed_set(clock_cpu_speed_t speed) {
#ifdef DEBUG
  const char* speeds[] = {
    "3.5", "7", "14", "28"
  };
#endif

  self.cpu_speed = speed;
  log_dbg("clock: CPU speed set to %s MHz\n", speeds[speed]);
}


clock_video_timing_t clock_video_timing_get(void) {
  return self.video_timing;
}


void clock_video_timing_set(clock_video_timing_t timing) {
#ifdef DEBUG
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
#endif

  self.video_timing = timing;
  log_dbg("clock: video timing set to %s\n", descriptions[timing]);
}


void clock_run(u32_t cpu_ticks) {
  const unsigned int clock_divider[E_CLOCK_CPU_SPEED_28MHZ - E_CLOCK_CPU_SPEED_3MHZ + 1] = {
    8, 4, 2, 1
  };
  const u32_t ticks_28mhz = cpu_ticks * clock_divider[self.cpu_speed];
  u32_t       ticks_14mhz;
  u32_t       ticks_2mhz;

  /* Update system clock. */
  self.ticks_28mhz += ticks_28mhz;

  /* Update 14 MHz clock for SLU. */
  ticks_14mhz = (self.ticks_28mhz - self.sync_14mhz) / 2;
  if (ticks_14mhz > 0) {
    ticks_14mhz = slu_run(ticks_14mhz);
    self.sync_14mhz += ticks_14mhz * 2;
  }

  /* Update 2 MHz clock for AY. */
  ticks_2mhz = (self.ticks_28mhz - self.sync_2mhz) / 16;
  if (ticks_2mhz > 0) {
    ay_run(ticks_2mhz);
    self.sync_2mhz += ticks_2mhz * 16;
  }

  if (self.ticks_28mhz >= self.sync_host_next) {
    main_sync();
    self.sync_host_next = self.ticks_28mhz + main_next_host_sync_get();
  }
}
