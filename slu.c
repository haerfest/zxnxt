#include <SDL2/SDL.h>
#include <string.h>
#include "defs.h"
#include "log.h"
#include "slu.h"
#include "tilemap.h"
#include "ula.h"


/**
 * A standard ULA screen (PAL) is generated like this, with pixels horizontally
 * and lines vertically:
 *
 * +-------------------------------------+
 * |                  ^                  |
 * |                 56                  |
 * |                  v                  |
 * |        +-------------------+        |
 * |        |                   |        |
 * |        |               ^   |        |
 * | < 32 > | < 256 >      192  | < 64 > | < 96 >
 * |        |               v   |        |
 * |        |                   |        |
 * |        +-------------------+        |
 * |                  ^                  |
 * |                 56                  |
 * |                  v                  |
 * +-------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * In terms of timing, a single horizontal line takes:
 *
 *   32 + 256 + 64 + 96 = 448 pixels.
 *
 * And a single screen takes:
 *
 *   56 + 192 + 56 + 8 = 312 lines.
 *
 * At a 7 MHz pixel-clock, that gives 312 * 448 / 7 MHz = 19.968 msec
 * per frame, or a frame rate of 50.08 Hz.
 *
 * The Next uses a 14 MHz pixel-clock since it has video modes that double the
 * horizontal resolution. For a high-resolution ULA screen (PAL) which has a
 * content area 512 pixels wide, that becomes:
 *
 * +--------------------------------------+
 * |                  ^                   |
 * |                 56                   |
 * |                  v                   |
 * |        +-------------------+         |
 * |        |                   |         |
 * |        |               ^   |         |
 * | < 64 > | < 512 >      192  | < 128 > | < 192 >
 * |        |               v   |         |
 * |        |                   |         |
 * |        +-------------------+         |
 * |                  ^                   |
 * |                 56                   |
 * |                  v                   |
 * +--------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * And for some of the other video modes the same total width, but slightly
 * different arrangement between borders and their content area of 640 pixels:
 *
 * +----------------------------+
 * |         ^                  |
 * |        56                  |
 * |         v                  |
 * +-------------------+        |
 * |                   |        |
 * |               ^   |        |
 * | < 640 >      192  | < 64 > | < 192 >
 * |               v   |        |
 * |                   |        |
 * +-------------------+        |
 * |         ^                  |
 * |        56                  |
 * |         v                  |
 * +----------------------------+
 *           ^
 *           8
 *           v
 *
 * These "layers" can be overlaid on top of one another. The resulting image
 * that is visible is 640 pixels x 256 lines, which is cut out as follows:
 *
 * For the default ULA screen:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                 32                  |        |
 * |        +-------------------+        |        |
 * |        |                   |        |        |
 * |        |               ^   |        |        |
 * | < 32 > | < 256 >      192  | < 32 > | < 32 > | < 96 >
 * |        |               v   |        |        |
 * |        |                   |        |        |
 * |        +-------------------+        |        |
 * |                 32                  |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * For the high resolution ULA screen:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                 32                  |        |
 * |        +-------------------+        |        |
 * |        |                   |        |        |
 * |        |               ^   |        |        |
 * | < 64 > | < 512 >      192  | < 64 > | < 64 > | < 192 >
 * |        |               v   |        |        |
 * |        |                   |        |        |
 * |        +-------------------+        |        |
 * |                 32                  |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * For the other video modes:
 *
 * +----------------------------------------------+
 * |                 24                           |
 * +-------------------------------------+        |
 * |                                     |        |
 * |                                     |        |
 * |                                     |        |
 * |                                 ^   |        |
 * | < 640 >                        256  | < 64 > | < 192 >
 * |                                 v   |        |
 * |                                     |        |
 * |                                     |        |
 * |                                     |        |
 * +-------------------------------------+        |
 * |                 24                           |
 * +----------------------------------------------+
 *                    ^
 *                    8
 *                    v
 *
 * The SLU blends all the layers together, so we need every layer to fill a
 * 640 x 256 frame buffer and return that so that the SLU can blend them.
 */


#define HORIZONTAL_RETRACE  192  /* Pixels or clock ticks. */
#define VERTICAL_RETRACE      8  /* Lines.                 */


typedef struct {
  SDL_Renderer*        renderer;
  SDL_Texture*         texture;
  u16_t*               blended_frame_buffer;
  slu_layer_priority_t layer_priority;
  u32_t                beam_row;
  u32_t                beam_column;
  int                  is_beam_visible;
} self_t;


static self_t self;


int slu_init(SDL_Renderer* renderer, SDL_Texture* texture) {
  memset(&self, 0, sizeof(self));

  self.blended_frame_buffer = malloc(FRAME_BUFFER_SIZE);
  if (self.blended_frame_buffer == NULL) {
    log_err("slu: out of memory\n");
    return -1;
  }

  self.renderer = renderer;
  self.texture  = texture;

  return 0;
}


void slu_finit(void) {
  if (self.blended_frame_buffer != NULL) {
    free(self.blended_frame_buffer);
    self.blended_frame_buffer = NULL;
  }
}


static void slu_blend_layers(void) {
  const u8_t* ula_frame_buffer = ula_frame_buffer_get();

  memcpy(self.blended_frame_buffer, ula_frame_buffer, FRAME_BUFFER_SIZE);
}


static void slu_blit(void) {
  SDL_Rect source_rect = {
    .x = 0,
    .y = 0,
    .w = WINDOW_WIDTH,
    .h = WINDOW_HEIGHT / 2
  };
  void* pixels;
  int   pitch;

  if (SDL_LockTexture(self.texture, NULL, &pixels, &pitch) != 0) {
    log_err("slu: SDL_LockTexture error: %s\n", SDL_GetError());
    return;
  }

  memcpy(pixels, self.blended_frame_buffer, FRAME_BUFFER_HEIGHT * FRAME_BUFFER_WIDTH * 2);
  SDL_UnlockTexture(self.texture);

  if (SDL_RenderCopy(self.renderer, self.texture, &source_rect, NULL) != 0) {
    log_err("slu: SDL_RenderCopy error: %s\n", SDL_GetError());
    return;
  }

  SDL_RenderPresent(self.renderer);
}


static int slu_beam_advance(void) {
  /* Advance beam one pixel horizontally. */
  self.beam_column++;

  if (self.beam_column < OVERSCAN_LEFT) {
    return 0;
  }

  if (self.beam_column < OVERSCAN_LEFT + FRAME_BUFFER_WIDTH) {
    return self.beam_row >= OVERSCAN_TOP && self.beam_row < OVERSCAN_TOP + FRAME_BUFFER_HEIGHT;
  }

  if (self.beam_column < OVERSCAN_LEFT + FRAME_BUFFER_WIDTH + OVERSCAN_RIGHT + HORIZONTAL_RETRACE) {
    return 0;
  }

  /* Move beam to beginning of next line. */
  self.beam_row++;
  self.beam_column = 0;

  if (self.beam_row < OVERSCAN_TOP) {
    return 0;
  }

  if (self.beam_row < OVERSCAN_TOP + FRAME_BUFFER_HEIGHT) {
    return 1;
  }

  if (self.beam_row < OVERSCAN_TOP + FRAME_BUFFER_HEIGHT + OVERSCAN_BOTTOM + VERTICAL_RETRACE) {
    return 0;
  }

  /* Return beam to top-left corner. */
  self.beam_row = 0;

  /* Update display. */
  slu_blend_layers();
  slu_blit();

  /* Notify layers that we completed a frame. */
  ula_did_complete_frame();

  return 0;
}


u32_t slu_run(u32_t ticks_14mhz) {
  u32_t tick;

  for (tick = 0; tick < ticks_14mhz; tick++) {
    if (slu_beam_advance()) {
      const u32_t beam_relative_row    = self.beam_row    - OVERSCAN_TOP;
      const u32_t beam_relative_column = self.beam_column - OVERSCAN_LEFT;

      ula_tick(beam_relative_row, beam_relative_column);
    }
  }

  /* TODO Deal with ULA using 7 MHz clock, not always consuming all ticks. */
  return ticks_14mhz;
}


void slu_layer_priority_set(slu_layer_priority_t priority) {
  self.layer_priority = priority;
}


slu_layer_priority_t slu_layer_priority_get(void) {
  return self.layer_priority;
}
