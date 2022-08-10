#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "debug.h"


typedef enum debug_cmd_t {
  E_DEBUG_CMD_NONE = 0,
  E_DEBUG_CMD_HELP,
  E_DEBUG_CMD_CONTINUE,
  E_DEBUG_CMD_QUIT,
  E_DEBUG_CMD_SHOW_REGISTERS
} debug_cmd_t;


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

  if (strcmp("r", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_REGISTERS;
    return 0;
  }

  return -1;
}


static void debug_show_registers(void) {
  const cpu_t* cpu = cpu_get();

  fprintf(stderr, " PC   SP   AF   BC   DE   HL   IX   IY   AF'  BC'  DE'  HL' SZ?H?PNC IM  IR  IFF1 IFF2\n");
  fprintf(stderr, "%04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %c%c%c%c%c%c%c%c %02x %04X %04X %04X\n",
    cpu->pc.w,
    cpu->sp.w,
    cpu->af.w,
    cpu->bc.w,
    cpu->de.w,
    cpu->hl.w,
    cpu->ix.w,
    cpu->iy.w,
    cpu->af_.w,
    cpu->bc_.w,
    cpu->de_.w,
    cpu->hl_.w,
    cpu->af.w & 0x80 ? '*' : '.',
    cpu->af.w & 0x40 ? '*' : '.',
    cpu->af.w & 0x20 ? '*' : '.',
    cpu->af.w & 0x10 ? '*' : '.',
    cpu->af.w & 0x08 ? '*' : '.',
    cpu->af.w & 0x04 ? '*' : '.',
    cpu->af.w & 0x02 ? '*' : '.',
    cpu->af.w & 0x01 ? '*' : '.',
    cpu->im,
    cpu->ir.w,
    cpu->iff1,
    cpu->iff2);
}


static void debug_execute(void) {
  switch (self.command) {
    case E_DEBUG_CMD_SHOW_REGISTERS:
      debug_show_registers();
      break;

    default:
      break;
  }
}


static void debug_prompt(void) {
    fprintf(stderr, "> ");
    fflush(stderr);
}


int debug_enter(void) {
  char input[80 + 1];

  debug_show_registers();

  for (;;) {
    debug_prompt();

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
