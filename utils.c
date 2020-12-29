#include <stdio.h>
#include "defs.h"
#include "log.h"


int utils_init(void) {
  return 0;
}


void utils_finit(void) {
}


int utils_load_rom(const char* filename, size_t expected_size, u8_t* buffer) {
  FILE* fp;
  long  size;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    log_err("utils: error opening %s\n", filename);
    goto exit;
  }

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  if (size != expected_size) {
    log_err("utils: size of %s is not %lu bytes\n", filename, expected_size);
    goto exit_file;
  }

  if (fread(buffer, size, 1, fp) != 1) {
    log_err("utils: error reading %s\n", filename);
    goto exit_file;
  }

  fclose(fp);

  return 0;

exit_file:
  fclose(fp);
exit:
  return -1;
}
