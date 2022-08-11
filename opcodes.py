#!/usr/bin/env python3


import io
import re
from tables import *
from typing import *


def generate(table: Table, prefix: List[Opcode]) -> Tuple[List[str], str]:
    fns  = []
    body = ''

    for opcode in sorted(table):
        pfx     = prefix + [opcode]
        pfx_str = ''.join(f'{n:02X}' for n in pfx)

        thing = table[opcode]
        if isinstance(thing, tuple):
            # An implementation.
            comment, fn = thing
            formatted   = ' '.join(fn().replace('\n', ' ').split())
            body += f'    case 0x{opcode:02X}: /* {comment:<14s} */ {{ {formatted} }} break;\n'

        elif isinstance(thing, dict):
            # Another table.
            sub_fns, sub_body = generate(thing, pfx)
            fns.extend(sub_fns)

            if pfx in [[0xDD, 0xCB], [0xFD, 0xCB]]:
                reader = '''
  TMP = memory_read(PC++); T(3);  /* displacement */
  const u8_t opcode = memory_read(PC); T(3);
  memory_contend(PC); T(1);
  memory_contend(PC); T(1);
  PC++;
'''
            else:
                reader = 'const u8_t opcode = memory_read(PC++); T(4);'

            fn = f'''
inline
static void execute_{pfx_str}(void) {{
  {reader.strip()}
  switch (opcode) {{
{sub_body.rstrip()}
    default:
      log_wrn("cpu: opcode {pfx_str}%02X at PC=$%04X not implemented\\n", opcode, PC - 1);
      break;
  }}
}}
'''
            fns.append(fn)
            body += f'    case 0x{opcode:02X}: execute_{pfx_str}(); break;\n'

        elif opcode in [0xDD, 0xFD] and prefix == [opcode]:
            # A repeated sequence of these builds one long uninterruptible opcode.
            me    = ''.join(f'{n:02X}' for n in prefix)
            body += f'    case 0x{opcode:02X}: execute_{me}(); break;\n'

        else:
            print(f'warning: no implementation of {pfx_str}')

    return fns, body


def main() -> None:
    fns, body = generate(table(), [])

    with open('opcodes.c', 'w') as f:
        for fn in fns:
            f.write(fn)

        f.write(f'''
inline
static void cpu_execute_next_opcode(void) {{
  R = (R & 0x80) | ((R + 1) & 0x7F);

  const u8_t opcode = memory_read_opcode(PC++); T(4);
  switch (opcode) {{
{body.rstrip()}
      log_wrn("cpu: opcode %02X at PC=$%04X not implemented\\n", opcode, PC - 1);
      break;
  }}
}}
''')


if __name__ == '__main__':
    main()
