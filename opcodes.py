#!/usr/bin/env python3


import io
from typing import *


Opcode      = int
Mnemonic    = str
Ticks       = int
Statement   = str
Instruction = Tuple[Mnemonic, Optional[Ticks], List[Statement]]
Table       = Dict[Opcode, Union[Instruction, 'Table']]


instructions: Table = {
    0xF3: ('DI', 4, [
        'cpu.iff1 = 0',
        'cpu.iff2 = 0'
    ]),
}


def generate(instructions: Table, f: io.TextIOBase, level: int = 0) -> None:
    spaces = ' ' * level * 2

    f.write(f'''
{spaces}const u8_t opcode = memory_read(cpu.pc++);
{spaces}switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, ticks, statements = item
            f.write(f'{spaces}  case 0x{opcode:02X}:  /* {mnemonic} */\n')
            for statement in statements:
                f.write(f'{spaces}    {statement};\n')
            if ticks:
                f.write(f'{spaces}    clock_ticks({ticks});\n')
            f.write(f'{spaces}    break;\n')
        else:
            f.write(f'{spaces}  case 0x{opcode:02X}:\n')
            generate(item, f, level + 1)
            f.write(f'{spaces}    break;\n')

    f.write(f'''
{spaces}  default:
{spaces}    fprintf(stderr, "Invalid opcode %02Xh at %04Xh\\n", opcode, cpu.pc - 1);
{spaces}    return -1;
}}''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
