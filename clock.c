#include <stdio.h>
#include "defs.h"
#include "clock.h"


typedef struct {
  unsigned        lines;
  unsigned        deci_tstates_per_line;
  clock_divider_t divider;
  unsigned        display_hz;
  unsigned        dot_clock_hz;  /* Product of all of the above. */
} timing_spec_t;


static const timing_spec_t timing_spec[E_CLOCK_TIMING_HDMI_60HZ - E_CLOCK_TIMING_VGA_50HZ_48K + 1] = {
  { 312, 2240, E_CLOCK_DIVIDER_3MHZ, 50, 27955200 },  /* E_CLOCK_TIMING_VGA_50HZ_48K */
  { 311, 2280, E_CLOCK_DIVIDER_3MHZ, 50, 28363200 },  /* E_CLOCK_TIMING_VGA_50HZ_128 */
  { 320, 2240, E_CLOCK_DIVIDER_3MHZ, 50, 28672000 },  /* E_CLOCK_TIMING_VGA_50HZ_PENTAGON */
  { 262, 2240, E_CLOCK_DIVIDER_3MHZ, 60, 28170240 },  /* E_CLOCK_TIMING_VGA_60HZ_48K */
  { 261, 2280, E_CLOCK_DIVIDER_3MHZ, 60, 28563840 },  /* E_CLOCK_TIMING_VGA_60HZ_128 */
  { 312, 2160, E_CLOCK_DIVIDER_3MHZ, 50, 26956800 },  /* E_CLOCK_TIMING_HDMI_50HZ */
  { 262, 2145, E_CLOCK_DIVIDER_3MHZ, 60, 26975520 }   /* E_CLOCK_TIMING_HDMI_60HZ */
};


typedef struct {
  clock_timing_t    timing;
  clock_cpu_speed_t cpu_speed;
  u64_t             ticks;  /* At max dot clock overflows in 20k years. */
} clock_t;


static clock_t clock;


int clock_init(void) {
  clock.timing    = E_CLOCK_TIMING_VGA_50HZ_128;
  clock.cpu_speed = E_CLOCK_CPU_SPEED_3MHZ;
  clock.ticks     = 0;

  return 0;
}


void clock_finit(void) {
}


/* Called by the CPU. */
void clock_ticks(unsigned cpu_ticks) {
  const unsigned ticks = cpu_ticks * timing_spec[clock.timing].divider;
  clock.ticks += ticks;

  /* TODO: run other hardware for 'ticks' ticks. */
}


void clock_cpu_speed_set(clock_cpu_speed_t speed) {
  const char* speeds[] = {
    "3.5", "7", "14", "28"
  };

  clock.cpu_speed = speed;
  fprintf(stderr, "clock: CPU speed set to %s MHz\n", speeds[speed]);
}
