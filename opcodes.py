#!/usr/bin/env python3


import io
from typing import *


Opcode         = int
Mnemonic       = str
CCode          = str
Ticks          = int
ConcreteSpec   = Union[CCode, Ticks]
ConcreteSpecs  = Union[ConcreteSpec, Sequence[ConcreteSpec]]
SpecGenerator  = Callable[[], ConcreteSpecs]
Specification  = Union[ConcreteSpec, SpecGenerator]
Specifications = Union[Specification, Sequence[Specification]]
Instruction    = Tuple[Mnemonic, Specifications]
Table          = Dict[Opcode, Union[Instruction, 'Table']]


def ld_dd_nn(dd: str) -> ConcreteSpecs:
    hi, lo = dd
    return [
        f'cpu.{lo} = memory_read(cpu.pc++)', 3,
        f'cpu.{hi} = memory_read(cpu.pc++)', 3,
    ]

def ld_hl_r(r: str) -> ConcreteSpecs:
    return [f'memory_write(cpu.h << 8 | cpu.l, cpu.{r})', 3]

def ld_r_n(r: str) -> ConcreteSpecs:
    return [f'cpu.{r} = memory_read(cpu.pc++)', 3]

def out_c_r(r: str) -> ConcreteSpecs:
    return [f'io_write(cpu.b << 8 | cpu.c, cpu.{r})', 4]


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
    0x11: ('LD DE,nn', lambda: ld_dd_nn('de')),
    0x16: ('LD D,n', lambda: ld_r_n('d')),
    0x21: ('LD HL,nn', lambda: ld_dd_nn('hl')),
    0x3E: ('LD A,n', lambda: ld_r_n('a')),
    0x75: ('LD (HL),L', lambda: ld_hl_r('l')),
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
    0xE6: ('AND n', [
        'cpu.a &= memory_read(cpu.pc++)',
        'cpu.f &= ~(FLAG_S | FLAG_Z | FLAG_P | FLAG_N | FLAG_C)',
        'cpu.f |= (cpu.a & 0x80) | (cpu.a == 0) << SHIFT_Z | FLAG_H | parity[cpu.a]',
        3
    ]),
    0xED: {
        0x51: ('OUT (C),D', lambda: out_c_r('d')),
        0x78: ('IN A,(C)', [
            'cpu.a = io_read(cpu.b << 8 | cpu.c)',
            'cpu.f &= ~(FLAG_S | FLAG_Z | FLAG_H | FLAG_V | FLAG_N)',
            'cpu.f |= (cpu.a & 0x80) | (cpu.a == 0) << SHIFT_Z | parity[cpu.a]',
            4
        ]),
        0x79: ('OUT (C),A', lambda: out_c_r('a')),
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
    0xF3: ('DI', 'cpu.iff1 = cpu.iff2 = 0')
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
            mnemonic, specifications = item
            f.write(f'{spaces}  case 0x{opcode:02X}:  /* {mnemonic} */\n')

            # Make our life easier by ensuring we always have a sequence.
            if not isinstance(specifications, list):
                specifications = [specifications]

            # Flatten the specifications by evaluating all generators first.
            concrete_specs = []
            for specification in specifications:
                if callable(specification):
                    concrete_specs.extend(specification())
                else:
                    concrete_specs.append(specification)

            for concrete_spec in concrete_specs:
                if isinstance(concrete_spec, int):
                    # Number of clock ticks.
                    if concrete_spec > 0:
                        f.write(f'{spaces}    clock_ticks({concrete_spec});\n')
                elif isinstance(concrete_spec, str):
                    # Line of C code.
                    f.write(f'{spaces}    {concrete_spec};\n')
                else:
                    raise RuntimeError(f'Unrecognised: {concrete_spec}')

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
