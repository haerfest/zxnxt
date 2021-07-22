#include <stdlib.h>
#include "audio.h"
#include "ay.h"
#include "clock.h"
#include "defs.h"
#include "log.h"
#include "main.h"
#include "slu.h"
#include "ula.h"


static const u32_t clock_28mhz[E_TIMING_LAST - E_TIMING_FIRST + 1] = {
  28000000,
  28571429,
  29464286,
  30000000,
  31000000,
  32000000,
  33000000,
  27000000
};


typedef struct clck_t {
  timing_t    clock_timing;
  cpu_speed_t cpu_speed;
  u64_t       ticks_28mhz;      /* At max dot clock overflows in 20k years. */
  u64_t       sync_14mhz;       /* Last 28 MHz tick where we synced the 14 MHz ULA clck. */
  u64_t       sync_2mhz;        /* Last 28 MHz tick where we synced the 1.75 MHz AY-3-8912 clck. */
  u64_t       sync_host_next;   /* Next moment at which to sync with reality. */
} clck_t;


static clck_t clck;


int clock_init(void) {
  clck.clock_timing   = E_TIMING_HDMI;
  clck.cpu_speed      = E_CPU_SPEED_3MHZ;
  clck.ticks_28mhz    = 0;
  clck.sync_14mhz     = clck.ticks_28mhz;
  clck.sync_2mhz      = clck.ticks_28mhz;
  clck.sync_host_next = clck.ticks_28mhz + main_next_host_sync_get(clock_28mhz[clck.clock_timing]);

  return 0;
}


void clock_finit(void) {
}


u64_t clock_ticks(void) {
  return clck.ticks_28mhz;
}


cpu_speed_t clock_cpu_speed_get(void) {
  return clck.cpu_speed;
}


void clock_cpu_speed_set(cpu_speed_t speed) {
  clck.cpu_speed = speed;

  main_show_cpu_speed(clck.cpu_speed);
}


inline
static void clock_run_28mhz_ticks(u64_t ticks) {
  u32_t ticks_14mhz;
  u32_t ticks_2mhz;

  /* Update system clck. */
  clck.ticks_28mhz += ticks;

  /* Update 14 MHz clock for SLU. */
  ticks_14mhz = (clck.ticks_28mhz - clck.sync_14mhz) / 2;
  if (ticks_14mhz > 0) {
    slu_run(ticks_14mhz);
    clck.sync_14mhz += ticks_14mhz * 2;
  }

  /* Update 2 MHz clock for AY. */
  ticks_2mhz = (clck.ticks_28mhz - clck.sync_2mhz) / 16;
  if (ticks_2mhz > 0) {
    ay_run(ticks_2mhz);
    clck.sync_2mhz += ticks_2mhz * 16;
  }

  if (clck.ticks_28mhz >= clck.sync_host_next) {
    main_sync();
    clck.sync_host_next = clck.ticks_28mhz + main_next_host_sync_get(clock_28mhz[clck.clock_timing]);
  }
}


inline
static void clock_run_inline(u32_t cpu_ticks) {
  const unsigned int clock_divider[E_CPU_SPEED_LAST - E_CPU_SPEED_FIRST + 1] = {
    8, 4, 2, 1
  };
  clock_run_28mhz_ticks(cpu_ticks * clock_divider[clck.cpu_speed]);
}


void clock_run(u32_t cpu_ticks) {
  const unsigned int clock_divider[E_CPU_SPEED_LAST - E_CPU_SPEED_FIRST + 1] = {
    8, 4, 2, 1
  };
  clock_run_28mhz_ticks(cpu_ticks * clock_divider[clck.cpu_speed]);
}


u32_t clock_28mhz_get(void) {
  return clock_28mhz[clck.clock_timing];
}


timing_t clock_timing_get(void) {
  return clck.clock_timing;
}


u8_t clock_timing_read(void) {
  return (u8_t) clck.clock_timing;
}


void clock_timing_write(u8_t value) {
  clck.clock_timing = value & 0x07;

  audio_clock_28mhz_set(clock_28mhz[clck.clock_timing]);
  ula_hdmi_enable(clck.clock_timing == E_TIMING_HDMI);

  main_show_timing(clck.clock_timing);
}
