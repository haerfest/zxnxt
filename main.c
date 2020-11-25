#include <SDL2/SDL.h>

#include "clock.h"
#include "cpu.h"


int main(int argc, char* argv[]) {
  SDL_Window* window = NULL;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  window = SDL_CreateWindow("twatwa", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (window == NULL) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    return 2;
  }
  
  clock_init();
  cpu_init();

  for (;;) {
    SDL_Event event;
  
    if (SDL_WaitEvent(&event) == 0) {
      SDL_Log("SDL_WaitEvent: %s\n", SDL_GetError());
      break;
    }

    if (event.type == SDL_QUIT) {
      break;
    }
  }
 
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
