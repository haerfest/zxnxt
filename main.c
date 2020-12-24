#include <SDL2/SDL.h>
#include "clock.h"
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"
#include "i2c.h"
#include "io.h"
#include "layer2.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "rom.h"
#include "sdcard.h"
#include "spi.h"
#include "timex.h"
#include "ula.h"
#include "utils.h"


#define FRAME_BUFFER_WIDTH  (32 + 256 + 64)
#define FRAME_BUFFER_HEIGHT 312

#define RENDER_SCALE_X 2
#define RENDER_SCALE_Y 2

#define WINDOW_WIDTH  (FRAME_BUFFER_WIDTH  * RENDER_SCALE_X)
#define WINDOW_HEIGHT (FRAME_BUFFER_HEIGHT * RENDER_SCALE_Y)


typedef struct {
  SDL_Window*   window;
  SDL_Renderer* renderer;
  SDL_Texture*  texture;
} main_t;


static main_t self;


static int main_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
    goto exit;
  }

  if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &self.window, &self.renderer) != 0) {
    fprintf(stderr, "main: SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (SDL_RenderSetScale(self.renderer, RENDER_SCALE_X, RENDER_SCALE_Y) != 0) {
    fprintf(stderr, "main: SDL_RenderSetScale error: %s\n", SDL_GetError());
    goto exit_window_and_renderer;
  }

  self.texture = SDL_CreateTexture(self.renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
  if (self.texture == NULL) {
    fprintf(stderr, "main: SDL_CreateTexture error: %s\n", SDL_GetError());
    goto exit_window_and_renderer;
  }

  if (utils_init() != 0) {
    goto exit_texture;
  }

  if (i2c_init() != 0) {
    goto exit_utils;
  }

  if (sdcard_init() != 0) {
    goto exit_i2c;
  }

  if (spi_init() != 0) {
    goto exit_sdcard;
  }

  if (nextreg_init() != 0) {
    goto exit_spi;
  }

  if (io_init() != 0) {
    goto exit_nextreg;
  }

  if (memory_init() != 0) {
    goto exit_io;
  }

  if (rom_init(memory_pointer(MEMORY_RAM_OFFSET_ZX_SPECTRUM_ROM), memory_pointer(MEMORY_RAM_OFFSET_ALTROM0_128K), memory_pointer(MEMORY_RAM_OFFSET_ALTROM1_48K)) != 0) {
    goto exit_memory;
  }

  if (mmu_init(memory_pointer(MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM)) != 0) {
    goto exit_rom;
  }

  if (divmmc_init(memory_pointer(MEMORY_RAM_OFFSET_DIVMMC_ROM), memory_pointer(MEMORY_RAM_OFFSET_DIVMMC_RAM)) != 0) {
    goto exit_mmu;
  }

  if (clock_init() != 0) {
    goto exit_divmmc;
  }

  if (ula_init(self.renderer, self.texture, memory_pointer(MEMORY_RAM_OFFSET_ZX_SPECTRUM_RAM)) != 0) {
    goto exit_clock;
  }

  if (timex_init() != 0) {
    goto exit_ula;
  }

  if (layer2_init() != 0) {
    goto exit_timex;
  }

  if (cpu_init() != 0) {
    goto exit_layer2;
  }

  return 0;

exit_layer2:
  layer2_finit();
exit_timex:
  timex_finit();
exit_ula:
  ula_finit();
exit_clock:
  clock_finit();
exit_divmmc:
  divmmc_finit();
exit_mmu:
  mmu_finit();
exit_rom:
  rom_finit();
exit_memory:
  memory_finit();
exit_io:
  io_finit();
exit_nextreg:
  nextreg_finit();
exit_spi:
  spi_finit();
exit_sdcard:
  sdcard_finit();
exit_i2c:
  i2c_finit();
exit_utils:
  utils_finit();
exit_texture:
  SDL_DestroyTexture(self.texture);
exit_window_and_renderer:
  SDL_DestroyRenderer(self.renderer);
  SDL_DestroyWindow(self.window);
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
  layer2_finit();
  timex_finit();
  ula_finit();
  clock_finit();
  divmmc_finit();
  mmu_finit();
  rom_finit();
  memory_finit();
  io_finit();
  nextreg_finit();
  spi_finit();
  sdcard_finit();
  i2c_finit();
  utils_finit();

  SDL_DestroyTexture(self.texture);
  SDL_DestroyRenderer(self.renderer);
  SDL_DestroyWindow(self.window);
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
