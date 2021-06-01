#ifndef __LOGGER_H
#define __LOGGER_H


#include <stdio.h>


/* Errors are always logged. */
#define log_err(...) fprintf(stderr, __VA_ARGS__)

#ifdef SILENT

#define log_wrn(...)
#define log_dbg(...)

#else  /* SILENT */

#define log_wrn(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
#define log_dbg(...) fprintf(stderr, __VA_ARGS__)
#else
#define log_dbg(...)
#endif

#endif  /* SILENT */


int  log_init(void);
void log_finit(void);


#endif  /* __LOGGER_H */
