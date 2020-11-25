#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "memory.h"


typedef struct {
  u8_t* memory;
  u8_t* rom;
} memory_t;


static memory_t memory;


static u8_t* load_rom(const char* filename, size_t expected_size) {
  FILE* fp;
  long  size;
  u8_t* rom;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Could not read %s\n", filename);
    goto exit;
  }

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  if (size != expected_size) {
    fprintf(stderr, "Size of %s is not %lu bytes\n", filename, expected_size);
    goto exit_file;
  }

  rom = malloc(size);
  if (rom == NULL) {
    fprintf(stderr, "Out of memory\n");
    goto exit_file;
  }

  if (fread(rom, size, 1, fp) != 1) {
    fprintf(stderr, "Error reading %s\n", filename);
    goto exit_rom;
  }

  fclose(fp);

  return rom;

exit_rom:
  free(rom);

exit_file:
  fclose(fp);

exit:
  return NULL;
}


int memory_init(void) {
  FILE *fp;
  long  size;

  memory.memory = calloc(1, 65536);
  if (memory.memory == NULL) {
    fprintf(stderr, "Out of memory\n");
    goto exit;
  }

  memory.rom = load_rom("enNextZX.rom", 64 * 1024);
  if (memory.rom == NULL) {
    goto exit_memory;
  }

  memcpy(memory.memory, memory.rom, 16 * 1024);

  return 0;

exit_memory:
  free(memory.memory);
  memory.memory = NULL;

exit:
  return -1;
}


void memory_finit(void) {
  if (memory.memory != NULL) {
    free(memory.memory);
  }
  if (memory.rom != NULL) {
    free(memory.rom);
  }
}


u8_t memory_read(u16_t address) {
  return memory.memory[address];
}
