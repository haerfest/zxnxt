#ifndef __BUFFER_H
#define __BUFFER_H


#include <SDL2/SDL.h>
#include "defs.h"


typedef struct {
  u8_t*      data;
  size_t     size;
  size_t     read_index;
  size_t     n_elements;
  SDL_mutex* mutex;
  SDL_cond*  not_full;
  SDL_cond*  not_empty;
} buffer_t;


int    buffer_init(buffer_t* buffer, size_t size);
void   buffer_finit(buffer_t* buffer);
void   buffer_reset(buffer_t* buffer);
size_t buffer_peek_n(buffer_t* buffer, size_t index, size_t n, u8_t* values);
size_t buffer_read_n(buffer_t* buffer, size_t n, u8_t* values);
size_t buffer_write(buffer_t* buffer, u8_t value);


#endif  /* __BUFFER_H */
