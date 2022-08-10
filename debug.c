#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "debug.h"
#include "memory.h"


typedef enum debug_cmd_t {
  E_DEBUG_CMD_NONE = 0,
  E_DEBUG_CMD_HELP,
  E_DEBUG_CMD_CONTINUE,
  E_DEBUG_CMD_QUIT,
  E_DEBUG_CMD_SHOW_REGISTERS,
  E_DEBUG_CMD_SHOW_MEMORY
} debug_cmd_t;


typedef struct debug_t {
  debug_cmd_t command;
  u16_t       address;
  u16_t       length;
} debug_t;


static debug_t self;


int debug_init(void) {
  memset(&self, 0, sizeof(self));
  self.length = 0x0100;
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

  if (strcmp("m", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_MEMORY;

    if (debug_next_word(q + 1, &p, &q)) {
      return 0;
    }
    self.address = strtol(p, NULL, 16);

    if (debug_next_word(q + 1, &p, &q)) {
      return 0;
    }
    self.length = strtol(p, NULL, 16);
    
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


static void debug_show_memory(void) {
  u16_t i, j;
  u8_t  byte;
  char  ascii[16 + 1];

  ascii[sizeof(ascii) - 1] = '\0';

  for (i = 0; i < self.length; i += 16) {
    fprintf(stderr, "%04X  ", self.address);    
    for (j = 0; j < 16; j++) {
      byte = memory_read(self.address++);
      fprintf(stderr, "%02X ", byte);
      ascii[j] = (byte < ' ' || byte > '~') ? '.' : byte;
    }
    fprintf(stderr, " %s\n", ascii);
  }
}


static const struct {
  debug_cmd_t command;
  void (*handler)(void);
} debug_commands[] = {
  { E_DEBUG_CMD_SHOW_REGISTERS, debug_show_registers },
  { E_DEBUG_CMD_SHOW_MEMORY,    debug_show_memory    }
};


static void debug_execute(void) {
  size_t i;

  for (i = 0; i < sizeof(debug_commands) / sizeof(*debug_commands); i++) {
    if (debug_commands[i].command == self.command) {
      debug_commands[i].handler();
      return;
    }
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
