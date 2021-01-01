#ifndef __LOGGER_H
#define __LOGGER_H


#include <stdio.h>


#define log_err(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
#define log_inf(...) fprintf(stderr, __VA_ARGS__)
#define log_wrn(...) fprintf(stderr, __VA_ARGS__)
#define log_dbg(...) fprintf(stderr, __VA_ARGS__)
#else
#define log_inf(...)
#define log_wrn(...)
#define log_dbg(...)
#endif


int  log_init(void);
void log_finit(void);


#endif  /* __LOGGER_H */
