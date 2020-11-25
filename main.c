#include <SDL2/SDL.h>

#include "clock.h"
#include "cpu.h"


static SDL_Window* window;


static int main_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
    return -1;
  }

  window = SDL_CreateWindow("twatwa", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (window == NULL) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    return -1;
  }

  clock_init();
  cpu_init();

  return 0;
}


static void main_eventloop(void) {
  SDL_Event event;
  
  for (;;) {
    if (SDL_WaitEvent(&event) == 0) {
      SDL_Log("SDL_WaitEvent: %s\n", SDL_GetError());
      break;
    }

    if (event.type == SDL_QUIT) {
      break;
    }
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
