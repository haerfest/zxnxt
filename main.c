#include <SDL2/SDL.h>

#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"
#include "io.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "ula.h"


static SDL_Window* window;


static int main_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
    goto exit;
  }

  window = SDL_CreateWindow("twatwa", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (window == NULL) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (nextreg_init() != 0) {
    goto exit_window;
  }

  if (divmmc_init() != 0) {
    goto exit_nextreg;
  }

  if (io_init() != 0) {
    goto exit_divmmc;
  }

  if (mmu_init() != 0) {
    goto exit_io;
  }

  if (memory_init() != 0) {
    goto exit_mmu;
  }

  if (clock_init() != 0) {
    goto exit_memory;
  }

  if (ula_init() != 0) {
    goto exit_clock;
  }

  if (cpu_init() != 0) {
    goto exit_ula;
  }

  return 0;

exit_ula:
  ula_finit();
exit_clock:
  clock_finit();
exit_memory:
  memory_finit();
exit_mmu:
  mmu_finit();
exit_io:
  io_finit();
exit_divmmc:
  divmmc_finit();
exit_nextreg:
  nextreg_finit();
exit_window:
  SDL_DestroyWindow(window);
exit_sdl:
  SDL_Quit();
exit:
  return -1;
}


static void main_eventloop(void) {
  SDL_Event event;
  u32_t     t0;
  u32_t     t1;
  u32_t     consumed;
  s32_t     ticks_left;

  ticks_left = 0;
  for (;;) {
    if (SDL_QuitRequested() == SDL_TRUE) {
      break;
    }

    t0 = SDL_GetTicks();
    if (cpu_run(1000000 + ticks_left, &ticks_left) != 0) {
      break;
    }
    t1 = SDL_GetTicks();

#if 0
    /* Assuming 3.5 MHz, 1,000,000 ticks take 286 milliseconds. */
    consumed = t1 - t0;
    SDL_Delay(286 - consumed);
#endif
  }
}


static void main_finit(void) {
  cpu_finit();
  ula_finit();
  clock_finit();
  memory_finit();
  mmu_finit();
  io_finit();
  divmmc_finit();
  nextreg_finit();

  SDL_DestroyWindow(window);
  SDL_Quit();
}


int main(int argc, char* argv[]) {
  if (main_init() != 0) {
    return 1;
  }

  main_eventloop();
  main_finit();

  return 0;
}
