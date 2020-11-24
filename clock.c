#include "defs.h"
#include "clock.h"


typedef enum {
  E_CPU_CLOCK_DIVIDER_28MHZ   = 1,
  E_CPU_CLOCK_DIVIDER_14MHZ   = 2,
  E_CPU_CLOCK_DIVIDER_7MHZ    = 4,
  E_CPU_CLOCK_DIVIDER_3500KHZ = 8
} cpu_clock_divider_t;


typedef enum {
  E_TIMING_VGA_50HZ_48K,
  E_TIMING_VGA_50HZ_128,
  E_TIMING_VGA_50HZ_PENTAGON,
  E_TIMING_VGA_60HZ_48K,
  E_TIMING_VGA_60HZ_128,
  E_TIMING_HDMI_50HZ,
  E_TIMING_HDMI_60HZ
} timing_t;


typedef struct {
  unsigned            lines;
  unsigned            deci_tstates_per_line;
  cpu_clock_divider_t cpu_clock_divider;
  unsigned            display_hz;
  unsigned            dot_clock_hz;  /* Product of all of the above. */
} timing_spec_t;


static const timing_spec_t timing_spec[E_TIMING_HDMI_60HZ - E_TIMING_VGA_50HZ_48K + 1] = {
  { 312, 2240, E_CPU_CLOCK_DIVIDER_3500KHZ, 50, 27955200 },  /* E_TIMING_VGA_50HZ_48K */
  { 311, 2280, E_CPU_CLOCK_DIVIDER_3500KHZ, 50, 28363200 },  /* E_TIMING_VGA_50HZ_128 */
  { 320, 2240, E_CPU_CLOCK_DIVIDER_3500KHZ, 50, 28672000 },  /* E_TIMING_VGA_50HZ_PENTAGON */
  { 262, 2240, E_CPU_CLOCK_DIVIDER_3500KHZ, 60, 28170240 },  /* E_TIMING_VGA_60HZ_48K */
  { 261, 2280, E_CPU_CLOCK_DIVIDER_3500KHZ, 60, 28563840 },  /* E_TIMING_VGA_60HZ_128 */
  { 312, 2160, E_CPU_CLOCK_DIVIDER_3500KHZ, 50, 26956800 },  /* E_TIMING_HDMI_50HZ */
  { 262, 2145, E_CPU_CLOCK_DIVIDER_3500KHZ, 60, 26975520 }   /* E_TIMING_HDMI_60HZ */
};


typedef struct {
  const timing_spec_t* timing_spec;
  cpu_clock_divider_t  cpu_clock_divider;
  u64_t                ticks;  /* At max dot clock overflows in 20k years. */
} clock_t;


static clock_t clock;


void clock_init(void) {
  clock.timing_spec       = &timing_spec[E_TIMING_HDMI_60HZ];
  clock.cpu_clock_divider = E_CPU_CLOCK_DIVIDER_3500KHZ;
  clock.ticks             = 0;
}


void clock_finit(void) {
}


/* Called by the CPU. */
void clock_tick(unsigned ticks) {
  clock.ticks += ticks;
}
