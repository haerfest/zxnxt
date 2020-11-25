#include <SDL2/SDL.h>

#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "memory.h"


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

  if (memory_init() != 0) {
    goto exit_window;
  }

  if (clock_init() != 0) {
    goto exit_memory;
  }

  if (cpu_init() != 0) {
    goto exit_clock;
  }

  return 0;

exit_clock:
  clock_finit();

exit_memory:
  memory_finit();

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
  
  for (;;) {
    if (SDL_PollEvent(&event) == 1) {
      if (event.type == SDL_QUIT) {
        break;
      }
    }

    t0 = SDL_GetTicks();
    cpu_run(100000);
    t1 = SDL_GetTicks();

    /* Assuming 3.5 MHz, 100,000 ticks take 28.6 milliseconds. */
    consumed = t1 - t0;
    SDL_Delay(28 - consumed);
  }
}


static void main_finit(void) {
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
