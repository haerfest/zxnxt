#ifndef __DEBUG_H
#define __DEBUG_H


typedef enum debug_cmd_t {
  E_DEBUG_CMD_NONE = 0,
  E_DEBUG_CMD_HELP,
  E_DEBUG_CMD_CONTINUE,
  E_DEBUG_CMD_QUIT
} debug_cmd_t;


int  debug_init(void);
void debug_finit(void);
int  debug_enter(void);


#endif  /* __DEBUG_H */