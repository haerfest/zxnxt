#include <SDL2/SDL.h>
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "cpu.h"
#include "defs.h"
#include "divmmc.h"
#include "i2c.h"
#include "io.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "palette.h"
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
  u8_t* sram;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
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

  if (log_init() != 0) {
    goto exit_texture;
  }

  if (utils_init() != 0) {
    goto exit_log;
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

  if (palette_init() != 0) {
    goto exit_spi;
  }

  if (nextreg_init() != 0) {
    goto exit_palette;
  }

  if (io_init() != 0) {
    goto exit_nextreg;
  }

  if (memory_init() != 0) {
    goto exit_io;
  }

  sram = memory_sram();

  if (bootrom_init(sram) != 0) {
    goto exit_memory;
  }

  if (config_init(sram) != 0) {
    goto exit_bootrom;
  }

  if (rom_init(sram) != 0) {
    goto exit_config;
  }

  if (mmu_init(sram) != 0) {
    goto exit_rom;
  }

  if (divmmc_init(sram) != 0) {
    goto exit_mmu;
  }

  if (clock_init() != 0) {
    goto exit_divmmc;
  }

  if (ula_init(self.renderer, self.texture, sram) != 0) {
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
exit_config:
  config_finit();
exit_bootrom:
  bootrom_finit();
exit_memory:
  memory_finit();
exit_io:
  io_finit();
exit_nextreg:
  nextreg_finit();
exit_palette:
  palette_finit();
exit_spi:
  spi_finit();
exit_sdcard:
  sdcard_finit();
exit_i2c:
  i2c_finit();
exit_utils:
  utils_finit();
exit_log:
  log_finit();
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
  for (;;) {
    if (SDL_QuitRequested() == SDL_TRUE) {
      break;
    }

    if (cpu_run(1000000) != 0) {
      break;
    }
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
  config_finit();
  bootrom_finit();
  memory_finit();
  io_finit();
  nextreg_finit();
  palette_finit();
  spi_finit();
  sdcard_finit();
  i2c_finit();
  utils_finit();
  log_finit();

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
