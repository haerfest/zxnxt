#ifndef __LOGGER_H
#define __LOGGER_H


#include <stdio.h>


/* Always want to show error messages. */
#define log_err(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG

#define log_dbg(...) fprintf(stderr, __VA_ARGS__)
#define log_inf(...) fprintf(stderr, __VA_ARGS__)
#define log_wrn(...) fprintf(stderr, __VA_ARGS__)

#else  /* DEBUG */

#define log_dbg(...)
#define log_inf(...)
#define log_wrn(...)

#endif  /* DEBUG */


int  log_init(void);
void log_finit(void);


#endif  /* __LOGGER_H */
