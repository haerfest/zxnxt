#include <SDL2/SDL.h>
#include "buffer.h"
#include "defs.h"
#include "log.h"


int buffer_init(buffer_t* buffer, size_t size) {
  buffer->size      = size;
  buffer->data      = malloc(size);
  buffer->mutex     = SDL_CreateMutex();
  buffer->element_added = SDL_CreateCond();
  buffer->element_removed  = SDL_CreateCond();

  if (buffer->data == NULL || buffer->mutex == NULL || buffer->element_added == NULL || buffer->element_removed == NULL) {
    log_wrn("buffer: out of memory\n");

    if (buffer->data) {
      free(buffer->data);
      buffer->data = NULL;
    }
    if (buffer->mutex) {
      SDL_DestroyMutex(buffer->mutex);
      buffer->mutex = NULL;
    }
    if (buffer->element_added) {
      SDL_DestroyCond(buffer->element_added);
      buffer->element_added = NULL;
    }
    if (buffer->element_removed) {
      SDL_DestroyCond(buffer->element_removed);
      buffer->element_removed = NULL;
    }

    return 1;
  }

  return 0;
}


void buffer_finit(buffer_t* buffer) {
  if (buffer->data) {
    free(buffer->data);
    buffer->data = NULL;
  }
  if (buffer->mutex) {
    SDL_DestroyMutex(buffer->mutex);
    buffer->mutex = NULL;
  }
  if (buffer->element_added) {
    SDL_DestroyCond(buffer->element_added);
    buffer->element_added = NULL;
  }
  if (buffer->element_removed) {
    SDL_DestroyCond(buffer->element_removed);
    buffer->element_removed = NULL;
  }
}


void buffer_reset(buffer_t* buffer) {
  buffer->read_index = 0;
  buffer->n_elements = 0;
}


size_t buffer_read_n(buffer_t* buffer, size_t n, u8_t* values) {
  size_t i;

  SDL_LockMutex(buffer->mutex);

  for (i = 0; buffer->n_elements && (i < n); i++) {
    if (values) {
      values[i] = buffer->data[buffer->read_index];
    }
    buffer->n_elements--;
    buffer->read_index = (buffer->read_index + 1) % buffer->size;
  }

  if (n > 0) {
    SDL_CondSignal(buffer->element_removed);
  }

  SDL_UnlockMutex(buffer->mutex);

  return i;
}


size_t buffer_peek_n(buffer_t* buffer, size_t index, size_t n, u8_t* values) {
  size_t i;

  SDL_LockMutex(buffer->mutex);

  for (i = 0; (index + i < buffer->n_elements) && (i < n); i++) {
    values[i] = buffer->data[(buffer->read_index + index + i) % buffer->size];
  }

  SDL_UnlockMutex(buffer->mutex);

  return i;
}


size_t buffer_write(buffer_t* buffer, u8_t value) {

  SDL_LockMutex(buffer->mutex);

  if (buffer->n_elements == buffer->size) {
    SDL_UnlockMutex(buffer->mutex);
    return 0;
  }

  buffer->data[(buffer->read_index + buffer->n_elements) % buffer->size] = value;
  buffer->n_elements++;

  SDL_CondSignal(buffer->element_added);

  SDL_UnlockMutex(buffer->mutex);
  
  return 1;
}


