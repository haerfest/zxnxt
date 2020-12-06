#ifndef __UTILS_H
#define __UTILS_H


#include <stdlib.h>
#include "defs.h"


int  utils_init(void);
void utils_finit(void);
int  utils_load_rom(const char* filename, size_t expected_size, u8_t* buffer);


#endif  /* __UTILS_H */
