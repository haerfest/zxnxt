#include <stdlib.h>
#include "ay.h"
#include "clock.h"
#include "defs.h"
#include "log.h"
#include "main.h"
#include "slu.h"


typedef struct {
  clock_cpu_speed_t cpu_speed;
  u64_t             ticks_28mhz;      /* At max dot clock overflows in 20k years. */
  u64_t             sync_14mhz;       /* Last 28 MHz tick where we synced the 14 MHz ULA clock. */
  u64_t             sync_2mhz;        /* Last 28 MHz tick where we synced the 1.75 MHz AY-3-8912 clock. */
  u64_t             sync_host_next;   /* Next moment at which to sync with reality. */
} self_t;


static self_t self;


int clock_init(void) {
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
