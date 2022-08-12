#!/usr/bin/env python3


import io
import re
from itertools import count
from tables    import *
from typing    import *


MAXARGS = 5


def format_instr(s: str) -> List[str]:
  '''
  Format an instruction by aligning the uppercase opcode and splitting the
  arguments on changes in case, e.g. "LD B,nn" => ["LD   ", "B,", "nn"].
  '''
  split    = s.split()
  mnemonic = split[0]
  parts    = [f'{mnemonic:<5}']

  args = ' '.join(split[1:])
  if args:
    part = args[0]
    
    for ch in args[1:]:
      if part[0].islower() == ch.islower():
        part += ch
      else:
        parts.append(part)
        part = ch
    
    parts.append(part)
  
  return parts


def arg_length(s: str) -> int:
  if s in ['e', 'd', 'n']:
    return 1
  if s in ['nn', 'mm']:
    return 2
  return 0

def disassemble(f: IO, t: Table, prefix: Optional[List[str]] = []) -> None:
    for opcode in range(256):
        if opcode in t and isinstance(t[opcode], dict):
            disassemble(f, t[opcode], prefix + [f'{opcode:02X}'])
    
    prefix_str = ''.join(prefix)
    f.write(f'static table_entry_t table{prefix_str}[256] = {{\n')

    for opcode in range(256):
        f.write(f'  /* 0x{opcode:02X} */ {{ ')
        if opcode in t:
            if isinstance(t[opcode], tuple):
                instr, _  = t[opcode]
                parts     = format_instr(instr)
                n         = len(prefix) + 1 + sum(arg_length(arg) for arg in parts[1:])
                formatted = ','.join(([f'"{part}"' for part in parts] + ['NULL'] * MAXARGS)[:MAXARGS])
                f.write(f'{n}, {{ .instr = {{{formatted}}} }}')
            elif isinstance(t[opcode], dict):
                f.write(f'0, {{ .table = table{prefix_str}{opcode:02X} }}')
            else:
                f.write('1, { .instr = {"?", NULL, NULL, NULL, NULL} }')
        else:
            f.write('1, { .instr = {"?", NULL, NULL, NULL, NULL} }')
        f.write(' },\n')
    
    f.write('};\n\n')


def main() -> None:
    with open('disassemble.c', 'w') as f:
        f.write(f'''
#define MAXARGS {MAXARGS}

typedef struct table_entry_t {{
  size_t length;
  union {{
    char*                 instr[MAXARGS];
    struct table_entry_t* table;
  }} payload;
}} table_entry_t;

''')

        disassemble(f, table())


if __name__ == '__main__':
    main()
