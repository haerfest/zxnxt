#include <SDL2/SDL.h>
#include "altrom.h"
#include "audio.h"
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "cpu.h"
#include "dac.h"
#include "defs.h"
#include "divmmc.h"
#include "i2c.h"
#include "io.h"
#include "keyboard.h"
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


#define RENDER_SCALE_X 2
#define RENDER_SCALE_Y 2

#define WINDOW_WIDTH  (FRAME_BUFFER_WIDTH  * RENDER_SCALE_X)
#define WINDOW_HEIGHT (FRAME_BUFFER_HEIGHT * RENDER_SCALE_Y)

#define RESET_HARD  1
#define RESET_SOFT  0


typedef struct {
  SDL_Window*       window;
  SDL_Renderer*     renderer;
  SDL_Texture*      texture;
  SDL_AudioDeviceID audio_device;
  const u8_t*       keyboard_state;
  SDL_bool          is_windowed;
} main_t;


static main_t self;


static int main_init(void) {
  SDL_AudioSpec want;
  SDL_AudioSpec have;
  u8_t*         sram;

  if (log_init() != 0) {
    goto exit;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    log_err("SDL_Init: %s\n", SDL_GetError());
    goto exit_log;
  }

  self.renderer = NULL;
  self.texture  = NULL;
  self.window   = NULL;

  memset(&want, 0, sizeof(want));
  want.freq     = AUDIO_SAMPLE_RATE;
  want.format   = AUDIO_S8;
  want.channels = 1;
  want.samples  = AUDIO_BUFFER_LENGTH;
  want.callback = audio_callback;
  
  self.audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (self.audio_device == 0) {
    log_err("main: SDL_OpenAudioDevice error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  self.window = SDL_CreateWindow("zxnxt", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (self.window == NULL) {
    log_err("main: SDL_CreateWindow error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  self.renderer = SDL_CreateRenderer(self.window, -1, SDL_RENDERER_ACCELERATED);
  if (self.renderer == NULL) {
    log_err("main: SDL_CreateRenderer error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (SDL_RenderSetIntegerScale(self.renderer, 1) != 0) {
    log_err("main: SDL_RenderSetIntegerScale error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (SDL_RenderSetLogicalSize(self.renderer, 352, 312) != 0) {
    log_err("main: SDL_RenderSetLogicalSize error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (SDL_RenderSetScale(self.renderer, RENDER_SCALE_X, RENDER_SCALE_Y) != 0) {
    log_err("main: SDL_RenderSetScale error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  self.texture = SDL_CreateTexture(self.renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STREAMING, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
  if (self.texture == NULL) {
    log_err("main: SDL_CreateTexture error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  self.keyboard_state = SDL_GetKeyboardState(NULL);
  self.is_windowed    = SDL_TRUE;

  if (audio_init(self.audio_device) != 0) {
    goto exit_sdl;
  }

  if (utils_init() != 0) {
    goto exit_audio;
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

  if (dac_init() != 0) {
    goto exit_io;
  }

  if (memory_init() != 0) {
    goto exit_dac;
  }

  sram = memory_sram();

  if (bootrom_init(sram) != 0) {
    goto exit_memory;
  }

  if (config_init(sram) != 0) {
    goto exit_bootrom;
  }

  if (altrom_init(sram) != 0) {
    goto exit_config;
  }

  if (rom_init(sram) != 0) {
    goto exit_altrom;
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

  if (keyboard_init() != 0) {
    goto exit_clock;
  }

  if (ula_init(self.renderer, self.texture, sram) != 0) {
    goto exit_keyboard;
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

  memory_refresh_accessors(0, 8);

  return 0;

exit_layer2:
  layer2_finit();
exit_timex:
  timex_finit();
exit_ula:
  ula_finit();
exit_keyboard:
  keyboard_finit();
exit_clock:
  clock_finit();
exit_divmmc:
  divmmc_finit();
exit_mmu:
  mmu_finit();
exit_rom:
  rom_finit();
exit_altrom:
  altrom_finit();
exit_config:
  config_finit();
exit_bootrom:
  bootrom_finit();
exit_memory:
  memory_finit();
exit_dac:
  dac_finit();
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
exit_audio:
  audio_finit();
exit_sdl:
  if (self.texture != NULL) {
    SDL_DestroyTexture(self.texture);
  }
  if (self.renderer != NULL) {
    SDL_DestroyRenderer(self.renderer);
  }
  if (self.window != NULL) {
    SDL_DestroyWindow(self.window);
  }
  SDL_CloseAudioDevice(self.audio_device);
  SDL_Quit();
exit_log:
  log_finit();
exit:
  return -1;
}


static void main_toggle_fullscreen(void) {
  const u32_t flags = self.is_windowed ? SDL_WINDOW_FULLSCREEN_DESKTOP: 0;
  
  if (SDL_SetWindowFullscreen(self.window, flags) != 0) {
    log_err("main: SDL_SetWindowFullscreen error: %s\n", SDL_GetError());
    return;
  }

  self.is_windowed = !self.is_windowed;
}


static void main_reset(int hard) {
  nextreg_select_write(NEXTREG_SELECT, E_NEXTREG_REGISTER_RESET);
  nextreg_data_write(NEXTREG_DATA, hard ? 0x02 : 0x01);
}


static void main_change_cpu_speed(void) {
  u8_t speed;

  /* TODO: Only allow this when enabled in peripheral 2 setting. */
  nextreg_select_write(NEXTREG_SELECT, E_NEXTREG_REGISTER_CPU_SPEED);
  speed = nextreg_data_read(NEXTREG_DATA) & 0x03;
  speed = speed + 1;
  nextreg_data_write(NEXTREG_DATA, speed & 0x03);
}


static void main_eventloop(void) {
  audio_resume();

  while (!SDL_QuitRequested()) {
    const u64_t audio_filled = clock_ticks() + 28000000L * AUDIO_BUFFER_LENGTH / AUDIO_SAMPLE_RATE;

    while (clock_ticks() < audio_filled) {
      cpu_step();
    }

    if (self.keyboard_state[SDL_SCANCODE_F1]) {
      main_reset(RESET_HARD);
    }

    if (self.keyboard_state[SDL_SCANCODE_F4]) {
      main_reset(RESET_SOFT);
    }

    if (self.keyboard_state[SDL_SCANCODE_F8]) {
      main_change_cpu_speed();
    }

    if (self.keyboard_state[SDL_SCANCODE_F12]) {
      main_toggle_fullscreen();
    }

    keyboard_refresh();
    audio_sync();
  }

  audio_pause();
}


static void main_finit(void) {
  cpu_finit();
  layer2_finit();
  timex_finit();
  ula_finit();
  keyboard_finit();
  clock_finit();
  divmmc_finit();
  mmu_finit();
  rom_finit();
  altrom_finit();
  config_finit();
  bootrom_finit();
  memory_finit();
  dac_finit();
  io_finit();
  nextreg_finit();
  palette_finit();
  spi_finit();
  sdcard_finit();
  i2c_finit();
  utils_finit();
  audio_finit();
  SDL_DestroyTexture(self.texture);
  SDL_DestroyRenderer(self.renderer);
  SDL_DestroyWindow(self.window);
  SDL_CloseAudioDevice(self.audio_device);
  SDL_Quit();
  log_finit();
}


int main(int argc, char* argv[]) {
  if (main_init() != 0) {
    return 1;
  }

  main_eventloop();
  main_finit();

  return 0;
}
