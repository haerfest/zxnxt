#include <stdio.h>
#include <string.h>
#include "debug.h"


typedef struct debug_t {
  debug_cmd_t command;
} debug_t;


static debug_t self;


int debug_init(void) {
  memset(&self, 0, sizeof(self));
  return 0;
}


void debug_finit(void) {
}


static int debug_next_word(char* s, char** p, char** q) {
  char* i = s;
  char* j;

  /* Skip spaces. */
  while (*i == ' ') {
    i++;
  }

  /* Nothing found when at end of input. */
  if (*i == '\n' || *i == '\0') {
    return -1;
  }

  /* Find end of word. */
  j = i + 1;
  while (*j != ' ' && *j != '\n' && *j != '\0') {
    j++;
  }

  /* Word is at s[i, j). */
  *p = i;
  *q = j;
  return 0;
}


static int debug_parse(char* s) {
  char *p;
  char *q;

  self.command = E_DEBUG_CMD_NONE;

  if (debug_next_word(s, &p, &q)) {
    return -1;
  }

  *q = '\0';

  if (strcmp("h", p) == 0 || strcmp("?", p) == 0) {
    self.command = E_DEBUG_CMD_HELP;
    return 0;
  }

  if (strcmp("c", p) == 0) {
    self.command = E_DEBUG_CMD_CONTINUE;
    return 0;
  }

  if (strcmp("q", p) == 0) {
    self.command = E_DEBUG_CMD_QUIT;
    return 0;
  }

  return -1;
}


static void debug_execute(void) {
}

int debug_enter(void) {
  char input[80 + 1];

  for (;;) {
    fprintf(stderr, "> ");
    fflush(stderr);

    if (fgets(input, sizeof(input), stdin)) {
      if (debug_parse(input) == 0) {
        switch (self.command) {
          case E_DEBUG_CMD_CONTINUE:
            return 0;
          case E_DEBUG_CMD_QUIT:
            return -1;
          default:
            debug_execute();
            break;
        }
      } else {
        fprintf(stderr, "Syntax error\n");
      }
    }
  }
}
