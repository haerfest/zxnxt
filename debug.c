#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "altrom.h"
#include "copper.h"
#include "cpu.h"
#include "debug.h"
#include "divmmc.h"
#include "memory.h"
#include "mf.h"
#include "mmu.h"
#include "nextreg.h"
#include "rom.h"


#include "disassemble.c"


typedef enum debug_cmd_t {
  E_DEBUG_CMD_NONE = 0,
  E_DEBUG_CMD_HELP,
  E_DEBUG_CMD_CONTINUE,
  E_DEBUG_CMD_QUIT,
  E_DEBUG_CMD_SHOW_REGISTERS,
  E_DEBUG_CMD_SHOW_MEMORY,
  E_DEBUG_CMD_SHOW_INFO,
  E_DEBUG_CMD_SHOW_DISASSEMBLY,
  E_DEBUG_CMD_SHOW_NEXT_REGISTERS,
  E_DEBUG_CMD_SHOW_COPPER,
  E_DEBUG_CMD_STEP,
  E_DEBUG_CMD_OVER,
  E_DEBUG_CMD_BREAKPOINTS_LIST,
  E_DEBUG_CMD_BREAKPOINTS_ADD,
  E_DEBUG_CMD_BREAKPOINTS_DELETE,
  E_DEBUG_CMD_ROM
} debug_cmd_t;


#define MAX_DEBUG_ARGS   10
#define MAX_BREAKPOINTS  11


typedef struct breakpoint_t {
  int   is_set;
  u16_t address;
} breakpoint_t;


typedef struct debug_t {
  debug_cmd_t command;
  int          nr_args;
  u16_t        args[MAX_DEBUG_ARGS];
  int          has_breakpoints;
  breakpoint_t breakpoints[MAX_BREAKPOINTS];  /* First one is for 'over' functionality. */
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

  if (debug_next_word(s, &p, &q)) {
    /* Repeat or continue last command. */
    return 0;
  }

  self.command = E_DEBUG_CMD_NONE;

  *q = '\0';

  if (strcmp("h", p) == 0 || strcmp("?", p) == 0) {
    self.command = E_DEBUG_CMD_HELP;
    return 0;
  }

  if (strcmp("c", p) == 0) {
    self.command = E_DEBUG_CMD_CONTINUE;
    self.nr_args = 0;
    if (debug_next_word(q + 1, &p, &q) == 0) {
      self.args[0] = strtol(p, NULL, 16);
      self.nr_args = 1;
    }
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

  if (strcmp("nr", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_NEXT_REGISTERS;
    return 0;
  }

  if (strcmp("s", p) == 0) {
    self.command = E_DEBUG_CMD_STEP;
    return 0;
  }

  if (strcmp("o", p) == 0) {
    self.command = E_DEBUG_CMD_OVER;
    return 0;
  }

  if (strcmp("m", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_MEMORY;
    if (debug_next_word(q + 1, &p, &q) == 0) {
      self.args[0] = strtol(p, NULL, 16);
    }
    return 0;
  }

  if (strcmp("i", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_INFO;
    return 0;
  }

  if (strcmp("d", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_DISASSEMBLY;
    if (debug_next_word(q + 1, &p, &q) == 0) {
      self.args[0] = strtol(p, NULL, 16);
    }
    return 0;
  }

  if (strcmp("bl", p) == 0) {
    self.command = E_DEBUG_CMD_BREAKPOINTS_LIST;
    return 0;
  }

  if (strcmp("b", p) == 0) {
    self.command = E_DEBUG_CMD_BREAKPOINTS_ADD;
    self.nr_args = 0;
    while (self.nr_args < MAX_DEBUG_ARGS && debug_next_word(q + 1, &p, &q) == 0) {
      self.args[self.nr_args++] = strtol(p, NULL, 16);
    }
    return 0;
  }

  if (strcmp("bd", p) == 0) {
    self.command = E_DEBUG_CMD_BREAKPOINTS_DELETE;
    self.nr_args = 0;
    while (self.nr_args < MAX_DEBUG_ARGS && debug_next_word(q + 1, &p, &q) == 0) {
      self.args[self.nr_args++] = strtol(p, NULL, 16);
    }
    return 0;
  }

  if (strcmp("rom", p) == 0) {
    self.command = E_DEBUG_CMD_ROM;
    self.nr_args = 0;
    if (debug_next_word(q + 1, &p, &q) == 0) {
      self.args[self.nr_args++] = strtol(p, NULL, 16);
    }
    return 0;
  }

  if (strcmp("cop", p) == 0) {
    self.command = E_DEBUG_CMD_SHOW_COPPER;
    return 0;
  }

  return -1;
}


static u16_t debug_disassemble(u16_t address) {
    fprintf(stderr, "%04X  ", address);

    table_entry_t* t      = table;
    size_t         length = 0;
    u8_t           opcode;

    /* Print any prefix + the opcode. */
    for (;;) {
      opcode = memory_read(address++);
      fprintf(stderr, "%02X ", opcode);
      length++;

      if (t[opcode].length > 0) {
        break;
      }

      t = t[opcode].payload.table;
    }

    /* Print remaining bytes of instruction and padding. */
    u16_t last_opcode_address = address;
    for (size_t i = length; i < t[opcode].length; i++) {
      fprintf(stderr, "%02X ", memory_read(address++));
    }
    for (size_t i = t[opcode].length; i < MAXARGS; i++) {
      fprintf(stderr, "   ");
    }

    address = last_opcode_address;
    for (int i = 0; i < MAXARGS; i++) {
      const char* s = t[opcode].payload.instr[i];
      if (s == NULL) {
        break;
      }
      if (strcmp(s, "e") == 0) {
        const s8_t offset = (s8_t) memory_read(address++);
        fprintf(stderr, "$%04X", address + offset);
        continue;
      }
      if (strcmp(s, "d") == 0) {
        fprintf(stderr, "$%02X", memory_read(address++));
        continue;
      }
      if (strcmp(s, "n") == 0) {
        fprintf(stderr, "$%02X", memory_read(address++));
        continue;
      }
      if (strcmp(s, "nn") == 0) {
        const u8_t lo = memory_read(address++);
        const u8_t hi = memory_read(address++);
        fprintf(stderr, "$%02X%02X", hi, lo);
        continue;
      }
      if (strcmp(s, "mm") == 0) {
        const u8_t hi = memory_read(address++);
        const u8_t lo = memory_read(address++);
        fprintf(stderr, "$%02X%02X", hi, lo);
        continue;
      }
      if (strcmp(s, "value") == 0) {
        fprintf(stderr, "$%02X", memory_read(address++));
        continue;
      }
      if (strcmp(s, "reg") == 0) {
        fprintf(stderr, "$%02X", memory_read(address++));
        continue;
      }

      fprintf(stderr, "%s", s);
    }

    fprintf(stderr, "\n");

    return address;
}


static int debug_show_registers(void) {
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

  return 0;
}


static int debug_show_next_registers(void) {
  u8_t value;

  fprintf(stderr, "   x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF\n");
  for (int i = 0; i < 256; i += 16) {
    fprintf(stderr, "%0Xx ", i >> 4);
    for (int j = 0; j < 16; j++) {
      const u8_t reg = i + j;
      (void) nextreg_read_internal(reg, &value);
      fprintf(stderr, "%02X ", value);
    }
    fprintf(stderr, "\n");
  }

  return 0;
}


static int debug_show_copper(void) {
  u16_t address;

  for (address = 0; address < 1024; address++) {
    const u16_t instr = copper_program_get(address);

    fprintf(stderr, "%03x: %04X ", address, instr);

    if (instr == 0x0000) {
      fprintf(stderr, "NOOP\n");
    } else if (instr == 0xFFFF) {
      fprintf(stderr, "HALT\n");
      break;
    } else if (instr & 0x8000) {
      const u32_t row = instr & 0x01FF;
      const u32_t col = (instr & 0x7E00) >> 6;
      fprintf(stderr, "WAIT %03X,%03X\n", row, col);
    } else {
      const u8_t reg   = (instr & 0x7F00) >> 8;
      const u8_t value = instr & 0x00FF;
      fprintf(stderr, "MOVE %02X,%02X\n", reg, value);
    }
  }

  return 0;
}


static int debug_continue(void) {
  if (self.nr_args > 0) {
    self.breakpoints[0].address = self.args[0];
    self.breakpoints[0].is_set  = 1;
    self.has_breakpoints        = 1;
  }

  /* Continue running. */
  return 1;
}


static int debug_step(void) {
  cpu_step();
  
  (void) debug_disassemble(cpu_get()->pc.w);

  return 0;
}


static int debug_instruction_length(u16_t address) {
    table_entry_t* t      = table;
    size_t         length = 0;
    u8_t           opcode;

    for (;;) {
      opcode = memory_read(address++);
      length++;

      if (t[opcode].length > 0) {
        break;
      }

      t = t[opcode].payload.table;
    }

    return length - 1 + t[opcode].length;
}


static int debug_over(void) {
  const u16_t pc   = cpu_get()->pc.w;
  const u16_t next = pc + debug_instruction_length(pc);
  
  self.breakpoints[0].address = next;
  self.breakpoints[0].is_set  = 1;
  self.has_breakpoints        = 1;

  /* Continue running. */
  return 1;
}


static int debug_show_info(void) {
  const char* reader;
  const char* writer;
  u8_t        mmu_page;

  fprintf(stderr, "ROM Alt MF DivMMC Auto Bank MapRAM\n");
  fprintf(stderr, "%2x   %c   %c   %c     %c   %2x     %c\n\n",
    rom_selected(),
    altrom_is_active_on_read() ? 'Y' : 'N',
    mf_is_active() ? 'Y' : 'N',
    divmmc_is_active() ? 'Y' : 'N',
    divmmc_is_automap_enabled() ? 'Y' : 'N',
    divmmc_bank(),
    divmmc_is_mapram_enabled() ? 'Y' : 'N');

  fprintf(stderr, "          page %12s %12s\n", "read", "write");
  for (int page = 0; page < 8; page++) {
    memory_describe_accessor(page, &reader, &writer);
    mmu_page = mmu_page_get(page);

    fprintf(stderr, "%04X-%04X  %02X  %12s %12s\n", page * 8192, (page + 1) * 8192 - 1, mmu_page, reader, writer);
  }

  return 0;
}


static int debug_show_memory(void) {
  u16_t i, j;
  u8_t  byte;
  char  ascii[16 + 1];

  ascii[sizeof(ascii) - 1] = '\0';

  for (i = 0; i < 256; i += 16) {
    fprintf(stderr, "%04X  ", self.args[0]);    
    for (j = 0; j < 16; j++) {
      byte = memory_read(self.args[0]++);
      fprintf(stderr, "%02X ", byte);
      ascii[j] = (byte < ' ' || byte > '~') ? '.' : byte;
    }
    fprintf(stderr, " %s\n", ascii);
  }

  return 0;
}


static int debug_show_disassembly(void) {
  for (int i = 0; i < 16; i++) {
    self.args[0] = debug_disassemble(self.args[0]);
  }

  return 0;
}


static int debug_breakpoints_list(void) {
  for (int i = 0; i < MAX_BREAKPOINTS; i++) {
    if (self.breakpoints[i].is_set) {
      fprintf(stderr, "$%04X\n", self.breakpoints[i].address);
    }
  }

  return 0;
}


static int debug_breakpoints_add(void) {
  for (int i = 0; i < self.nr_args; i++) {
    const u16_t address = self.args[i];
    /* Add except for the 'over' breakpoint. */
    for (int j = 1; j < MAX_BREAKPOINTS; j++) {
      if (!self.breakpoints[j].is_set || self.breakpoints[j].address == address) {
        self.breakpoints[j].address = address;
        self.breakpoints[j].is_set  = 1;
        self.has_breakpoints        = 1;
        break;
      }
    }
  }

  return 0;
}


static void debug_set_has_breakpoints(void) {
  self.has_breakpoints = 0;
  for (int j = 0; j < MAX_BREAKPOINTS; j++) {
    self.has_breakpoints |= self.breakpoints[j].is_set;
  }
}


static int debug_breakpoints_delete(void) {
  for (int i = 0; i < self.nr_args; i++) {
    const u16_t address = self.args[i];
    for (int j = 0; j < MAX_BREAKPOINTS; j++) {
      if (self.breakpoints[j].address == address) {
        self.breakpoints[j].is_set = 0;
        break;
      }
    }
  }

  debug_set_has_breakpoints();

  return 0;
}


static int debug_rom(void) {
  if (self.nr_args != 1) {
    fprintf(stderr, "ROM %u\n", rom_selected());
    return 0;
  }
  if (self.args[0] < E_ROM_FIRST || self.args[1] > E_ROM_LAST) {
    fprintf(stderr, "Invalid ROM #\n");
    return 0;
  }
  rom_select((rom_t) self.args[0]);
  return 0;
}


int debug_is_breakpoint(u16_t address) {
  if (!self.has_breakpoints) {
    return 0;
  }

  for (int j = 0; j < MAX_BREAKPOINTS; j++) {
    if (self.breakpoints[j].is_set && self.breakpoints[j].address == address) {
      return 1;
    }
  }

  return 0;
}


static const struct {
  debug_cmd_t command;
  int (*handler)(void);
} debug_commands[] = {
  { E_DEBUG_CMD_BREAKPOINTS_ADD,     debug_breakpoints_add     },
  { E_DEBUG_CMD_BREAKPOINTS_DELETE,  debug_breakpoints_delete  },
  { E_DEBUG_CMD_BREAKPOINTS_LIST,    debug_breakpoints_list    },
  { E_DEBUG_CMD_CONTINUE,            debug_continue            },
  { E_DEBUG_CMD_OVER,                debug_over                },
  { E_DEBUG_CMD_ROM,                 debug_rom                 },
  { E_DEBUG_CMD_SHOW_DISASSEMBLY,    debug_show_disassembly    },
  { E_DEBUG_CMD_SHOW_INFO,           debug_show_info           },
  { E_DEBUG_CMD_SHOW_MEMORY,         debug_show_memory         },
  { E_DEBUG_CMD_SHOW_NEXT_REGISTERS, debug_show_next_registers },
  { E_DEBUG_CMD_SHOW_REGISTERS,      debug_show_registers      },
  { E_DEBUG_CMD_SHOW_COPPER,         debug_show_copper         },
  { E_DEBUG_CMD_STEP,                debug_step                }
};


static int debug_execute(void) {
  size_t i;

  for (i = 0; i < sizeof(debug_commands) / sizeof(*debug_commands); i++) {
    if (debug_commands[i].command == self.command) {
      return debug_commands[i].handler();
    }
  }

  return 0;
}


static void debug_prompt(void) {
  fprintf(stderr, "> ");
  fflush(stderr);
}


int debug_enter(void) {
  char input[80 + 1];

  (void) debug_disassemble(cpu_get()->pc.w);

  /* When we completed 'over', clear its breakpoint. */
  if (self.breakpoints[0].is_set && cpu_get()->pc.w == self.breakpoints[0].address) {
    self.breakpoints[0].is_set = 0;
    debug_set_has_breakpoints();
  }

  self.command = E_DEBUG_CMD_NONE;
  
  for (;;) {
    debug_prompt();

    if (fgets(input, sizeof(input), stdin) == NULL) {
      return -1;
    }

    if (debug_parse(input) != 0) {
      fprintf(stderr, "Syntax error\n");
      continue;
    }
    if (self.command == E_DEBUG_CMD_QUIT) {
      return -1;
    }
    if (debug_execute()) {
      return 0;
    }
  }
}
