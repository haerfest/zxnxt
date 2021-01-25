#include <SDL2/SDL.h>
#include "altrom.h"
#include "audio.h"
#include "ay.h"
#include "bootrom.h"
#include "clock.h"
#include "config.h"
#include "cpu.h"
#include "dac.h"
#include "defs.h"
#include "divmmc.h"
#include "i2c.h"
#include "io.h"
#include "kempston.h"
#include "keyboard.h"
#include "layer2.h"
#include "log.h"
#include "memory.h"
#include "mmu.h"
#include "nextreg.h"
#include "paging.h"
#include "palette.h"
#include "rom.h"
#include "sdcard.h"
#include "spi.h"
#include "ula.h"
#include "utils.h"


/* Tasks we want to schedule after the CPU has finished an instruction. */
typedef enum {
  E_MAIN_TASK_NONE,
  E_MAIN_TASK_RESET_HARD,
  E_MAIN_TASK_RESET_SOFT,
  E_MAIN_TASK_CHANGE_CPU_SPEED,
  E_MAIN_TASK_QUIT
} main_task_t;

  
typedef struct {
  SDL_Window*         window;
  SDL_Renderer*       renderer;
  SDL_Texture*        texture;
  SDL_AudioDeviceID   audio_device;
  SDL_GameController* controller;
  const u8_t*         keyboard_state;
  int                 is_windowed;
  int                 is_function_key_down;
  main_task_t         task;
} self_t;


static self_t self;


static int main_init(void) {
  SDL_AudioSpec want;
  SDL_AudioSpec have;
  u8_t*         sram;
  int           i;

  memset(&self, 0, sizeof(self));

  if (log_init() != 0) {
    goto exit;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
    log_err("SDL_Init: %s\n", SDL_GetError());
    goto exit_log;
  }

  self.renderer   = NULL;
  self.texture    = NULL;
  self.window     = NULL;
  self.controller = NULL;

  memset(&want, 0, sizeof(want));
  want.freq     = AUDIO_SAMPLE_RATE;
  want.format   = AUDIO_S8;
  want.channels = AUDIO_N_CHANNELS;
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

  if (SDL_RenderSetLogicalSize(self.renderer, WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
    log_err("main: SDL_RenderSetLogicalSize error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  if (SDL_RenderSetScale(self.renderer, 1, 1) != 0) {
    log_err("main: SDL_RenderSetScale error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  self.texture = SDL_CreateTexture(self.renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STREAMING, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
  if (self.texture == NULL) {
    log_err("main: SDL_CreateTexture error: %s\n", SDL_GetError());
    goto exit_sdl;
  }

  log_inf("main: %d joystick/gamecontrollers attached\n", SDL_NumJoysticks());
  for (i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i)) {
      self.controller = SDL_GameControllerOpen(i);
      if (self.controller) {
        log_inf("main: %s detected\n", SDL_GameControllerName(self.controller));
        break;
      }
    }
  }

  (void) SDL_GameControllerEventState(SDL_ENABLE);

  (void) SDL_ShowCursor(SDL_DISABLE);
  
  self.keyboard_state       = SDL_GetKeyboardState(NULL);
  self.is_function_key_down = 0;
  self.is_windowed          = 1;

  if (audio_init(self.audio_device) != 0) {
    goto exit_sdl;
  }

  if (ay_init() != 0) {
    goto exit_audio;
  }

  if (kempston_init(self.controller) != 0) {
    goto exit_ay;
  }

  if (utils_init() != 0) {
    goto exit_kempston;
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

  if (paging_init() != 0) {
    goto exit_mmu;
  }

  if (divmmc_init(sram) != 0) {
    goto exit_paging;
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

  if (layer2_init() != 0) {
    goto exit_ula;
  }

  if (cpu_init() != 0) {
    goto exit_layer2;
  }

  memory_refresh_accessors(0, 8);

  return 0;

exit_layer2:
  layer2_finit();
exit_ula:
  ula_finit();
exit_keyboard:
  keyboard_finit();
exit_clock:
  clock_finit();
exit_divmmc:
  divmmc_finit();
exit_paging:
  paging_finit();
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
exit_kempston:
  kempston_finit();
exit_ay:
  ay_finit();
exit_audio:
  audio_finit();
exit_sdl:
  if (self.controller != NULL) {
    SDL_GameControllerClose(self.controller);
  }
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
  const u32_t flags = self.is_windowed ? SDL_WINDOW_FULLSCREEN: 0;
  
  if (SDL_SetWindowFullscreen(self.window, flags) != 0) {
    log_err("main: SDL_SetWindowFullscreen error: %s\n", SDL_GetError());
    return;
  }

  self.is_windowed = !self.is_windowed;

  if (self.is_windowed) {
    SDL_SetWindowSize(self.window, WINDOW_WIDTH, WINDOW_HEIGHT);
  }
}


static void main_reset(int hard) {
  const u8_t selected = nextreg_select_read(NEXTREG_SELECT);
  nextreg_select_write(NEXTREG_SELECT, E_NEXTREG_REGISTER_RESET);
  nextreg_data_write(NEXTREG_DATA, hard ? 0x02 : 0x01);
  nextreg_select_write(NEXTREG_SELECT, selected);
}


static void main_change_cpu_speed(void) {
  const u8_t selected = nextreg_select_read(NEXTREG_SELECT);
  u8_t       speed;

  /* TODO: Only allow this when enabled in peripheral 2 setting. */
  nextreg_select_write(NEXTREG_SELECT, E_NEXTREG_REGISTER_CPU_SPEED);
  speed = nextreg_data_read(NEXTREG_DATA) & 0x03;
  speed = speed + 1;
  nextreg_data_write(NEXTREG_DATA, speed & 0x03);
  nextreg_select_write(NEXTREG_SELECT, selected);
}


static void main_dump_memory(void) {
  const u16_t pc = cpu_pc_get();
  FILE*       fp;
  u32_t       address;
  char        filename[18 + 1];

  (void) snprintf(filename, sizeof(filename), "memory-PC=%04X.bin", pc);
  fp = fopen(filename, "wb");
  if (fp == NULL) {
    log_err("main: could not open %s for writing\n", filename);
    return;
  }

  for (address = 0x0000; address <= 0xFFFF; address++) {
    fputc(memory_read(address), fp);
  }

  fclose(fp);
  log_inf("main: %s written\n", filename);
}


static void main_handle_function_keys(void) {
  const int f1  = self.keyboard_state[SDL_SCANCODE_F1];
  const int f4  = self.keyboard_state[SDL_SCANCODE_F4];
  const int f8  = self.keyboard_state[SDL_SCANCODE_F8];
  const int f11 = self.keyboard_state[SDL_SCANCODE_F11];
  const int f12 = self.keyboard_state[SDL_SCANCODE_F12];

  if (f1 || f4 || f8 || f11 || f12) {
    if (!self.is_function_key_down) {
      if (f1)  self.task = E_MAIN_TASK_RESET_HARD;
      if (f4)  self.task = E_MAIN_TASK_RESET_SOFT;
      if (f8)  self.task = E_MAIN_TASK_CHANGE_CPU_SPEED;
      if (f11) main_dump_memory();
      if (f12) main_toggle_fullscreen();

      /* Prevent auto-repeat from continuous triggering. */
      self.is_function_key_down = 1;
    }
  } else if (self.is_function_key_down) {
    self.is_function_key_down = 0;
  }
}


static void main_eventloop(void) {
  audio_resume();

  while (self.task != E_MAIN_TASK_QUIT) {

    while (self.task == E_MAIN_TASK_NONE) {
      cpu_step();
    }

    switch (self.task) {
      case E_MAIN_TASK_RESET_HARD:
      case E_MAIN_TASK_RESET_SOFT:
        main_reset(self.task == E_MAIN_TASK_RESET_HARD);
        self.task = E_MAIN_TASK_NONE;
        break;

      case E_MAIN_TASK_CHANGE_CPU_SPEED:
        main_change_cpu_speed();
        self.task = E_MAIN_TASK_NONE;
        break;

      default:
        break;
    }
  }

  audio_pause();
}


static void main_finit(void) {
  cpu_finit();
  layer2_finit();
  ula_finit();
  keyboard_finit();
  clock_finit();
  divmmc_finit();
  paging_finit();
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
  kempston_finit();
  ay_finit();
  audio_finit();
  if (self.controller) {
    SDL_GameControllerClose(self.controller);
  }
  SDL_DestroyTexture(self.texture);
  SDL_DestroyRenderer(self.renderer);
  SDL_DestroyWindow(self.window);
  SDL_CloseAudioDevice(self.audio_device);
  SDL_Quit();
  log_finit();
}


u32_t main_next_host_sync_get(void) {
  return (unsigned long) clock_28mhz_get() * AUDIO_BUFFER_LENGTH / AUDIO_SAMPLE_RATE;
}


void main_sync(void) {
  /* Perform these housekeeping tasks in downtime. */
  kempston_refresh();
  keyboard_refresh();
  main_handle_function_keys();

  if (SDL_QuitRequested()) {
    self.task = E_MAIN_TASK_QUIT;
  }

  audio_sync();
}


int main(int argc, char* argv[]) {
  if (main_init() != 0) {
    return 1;
  }

  main_eventloop();
  main_finit();

  return 0;
}
