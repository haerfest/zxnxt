#!/usr/bin/env python3


import io
import re
from itertools import count
from tables    import *
from typing    import *


def split_on_lowercase(s: str) -> List[str]:
  parts = []

  part = s[0]
  for ch in s[1:]:
    if part[0].islower() == ch.islower():
      part += ch
    else:
      parts.append(part)
      part = ch
  
  parts.append(part)
  return parts


def disassemble(f: IO, t: Table, prefix: Optional[str] = '') -> None:
    for opcode in range(256):
        if opcode in t and isinstance(t[opcode], dict):
            disassemble(f, t[opcode], f'{prefix}{opcode:02X}')
    
    f.write(f'static table_entry_t table{prefix}[256] = {{\n')

    for opcode in range(256):
        f.write(f'  /* 0x{opcode:02X} */ {{ ')
        if opcode in t:
            if isinstance(t[opcode], tuple):
                instr, _ = t[opcode]
                parts    = [f'"{s}"' for s in split_on_lowercase(instr)] + ['NULL'] * 4
                joined   = ",".join(parts[:4])
                f.write(f'0, {{ .instr = {{{joined}}} }}')
            elif isinstance(t[opcode], dict):
                f.write(f'1, {{ .table = table{prefix}{opcode:02X} }}')
            else:
                f.write('0, { .instr = {"?", NULL, NULL, NULL} }')
        else:
            f.write('0, { .instr = {"?", NULL, NULL, NULL} }')
        f.write(' },\n')
    
    f.write('};\n\n')


def main() -> None:
    with open('disassemble.c', 'w') as f:
        f.write(f'''
typedef struct table_entry_t {{
  int is_table;
  union {{
    char*                 instr[4];
    struct table_entry_t* table;
  }} payload;
}} table_entry_t;

''')

        disassemble(f, table())


if __name__ == '__main__':
    main()
