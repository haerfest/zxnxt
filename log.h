#ifndef __LOGGER_H
#define __LOGGER_H


#include <SDL2/SDL.h>


int  log_init(void);
void log_finit(void);


#ifdef DEBUG

#define log_dbg(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define log_err(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define log_inf(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,  __VA_ARGS__)
#define log_wrn(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,  __VA_ARGS__)

#else  /* DEBUG */

#define log_dbg(...)
#define log_err(...)
#define log_inf(...)
#define log_wrn(...)

#endif  /* DEBUG */


#endif  /* __LOGGER_H */
