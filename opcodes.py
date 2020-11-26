#!/usr/bin/env python3


import io
from typing import *


Opcode      = int
Mnemonic    = str
Ticks       = int
Statement   = str
Instruction = Tuple[Mnemonic, List[Union[Statement, Ticks]]]
Table       = Dict[Opcode, Union[Instruction, 'Table']]


instructions: Table = {
    0x3E: ('LD A,n', [
        'cpu.a = memory_read(cpu.pc)',      4,
        'cpu.pc++',                         3,
    ]),
    0xC3: ('JP nn', [
        'cpu.z  = memory_read(cpu.pc)',     4,
        'cpu.w  = memory_read(cpu.pc + 1)', 3,
        'cpu.pc = cpu.w << 8 | cpu.z',      3,
    ]),
    0xED: {
        0x91: ('NEXTREG reg,value', [
            'io_write(0x243B, memory_read(cpu.pc++))',
            'io_write(0x253B, memory_read(cpu.pc++))',
            16,

        ]),
        0x92: ('NEXTREG reg,A', [
            'io_write(0x243B, memory_read(cpu.pc++))',
            'io_write(0x253B, cpu.a)',
            12
        ]),
    },
    0xF3: ('DI', [
        'cpu.iff1 = cpu.iff2 = 0', 4
    ]),
}


def generate(instructions: Table, f: io.TextIOBase, level: int = 0, prefix: Optional[List[Opcode]] = None) -> None:
    prefix = prefix or []
    spaces = ' ' * level * 4

    f.write(f'''{spaces}opcode = memory_read(cpu.pc++);
{spaces}switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, statements = item
            f.write(f'{spaces}  case 0x{opcode:02X}:  /* {mnemonic} */\n')
            for statement in statements:
                if isinstance(statement, int):
                    f.write(f'{spaces}    clock_ticks({statement});\n')
                else:
                    f.write(f'{spaces}    {statement};\n')
            f.write(f'{spaces}    break;\n\n')
        else:
            f.write(f'{spaces}  case 0x{opcode:02X}:\n')
            generate(item, f, level + 1, prefix + [opcode])
            f.write(f'{spaces}    break;\n\n')

    s = ''.join(f'{opcode:02X}h ' for opcode in prefix)
    n = len(prefix) + 1
    f.write(f'''{spaces}  default:
{spaces}    fprintf(stderr, "Invalid opcode {s}%02Xh at %04Xh\\n", opcode, cpu.pc - {n});
{spaces}    return -1;
{spaces}}}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
