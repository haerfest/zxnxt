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
    0x01: ('LD BC,nn', [
        'cpu.c = memory_read(cpu.pc++)', 3,
        'cpu.b = memory_read(cpu.pc++)', 3
    ]),
    0x04: ('INC B', [
        'cpu.b++',
        'cpu.f &= ~(FLAG_S | FLAG_Z | FLAG_H | FLAG_V | FLAG_N)',
        'cpu.f |= (cpu.b & 0x80) | (cpu.b == 0) << SHIFT_Z | (((cpu.b - 1) & 0x0F) + 1) & 0x10 | (cpu.b == 0x80) << SHIFT_V'
    ]),
    0x16: ('LD D,n', [
        'cpu.d = memory_read(cpu.pc++)', 3
    ]),
    0x3E: ('LD A,n', [
        'cpu.a = memory_read(cpu.pc++)', 3
    ]),
    0xAF: ('XOR A', [
        'cpu.a = 0',
        'cpu.f &= ~(FLAG_S | FLAG_H | FLAG_N | FLAG_C)',
        'cpu.f |= FLAG_Z | FLAG_P'
    ]),
    0xC3: ('JP nn', [
        'cpu.z  = memory_read(cpu.pc)',     3,
        'cpu.w  = memory_read(cpu.pc + 1)', 3,
        'cpu.pc = cpu.w << 8 | cpu.z'
    ]),
    0xED: {
        0x51: ('OUT (C),D', [
            'io_write(cpu.b << 8 | cpu.c, cpu.d)', 4
        ]),
        0x78: ('IN A,(C)', [
            'cpu.a = io_read(cpu.b << 8 | cpu.c)',
            'cpu.f &= ~(FLAG_S | FLAG_Z | FLAG_H | FLAG_V | FLAG_N)',
            'cpu.f |= (cpu.a & 0x80) | (cpu.a == 0) << SHIFT_Z | parity[cpu.a]',
            4
        ]),
        0x91: ('NEXTREG reg,value', [
            'io_write(0x243B, memory_read(cpu.pc++))',
            'io_write(0x253B, memory_read(cpu.pc++))',
            8
        ]),
        0x92: ('NEXTREG reg,A', [
            'io_write(0x243B, memory_read(cpu.pc++))',
            'io_write(0x253B, cpu.a)',
            4
        ]),
    },
    0xF3: ('DI', [
        'cpu.iff1 = cpu.iff2 = 0'
    ]),
}


def generate(instructions: Table, f: io.TextIOBase, level: int = 0, prefix: Optional[List[Opcode]] = None) -> None:
    prefix = prefix or []
    spaces = ' ' * level * 4

    f.write(f'''{spaces}opcode = memory_read(cpu.pc++);
{spaces}clock_ticks(4);
{spaces}switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, statements = item
            f.write(f'{spaces}  case 0x{opcode:02X}:  /* {mnemonic} */\n')
            for item in statements:
                if isinstance(item, int):
                    if item > 0:
                        f.write(f'{spaces}    clock_ticks({item});\n')
                else:
                    f.write(f'{spaces}    {item};\n')
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
