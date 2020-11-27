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
        f'{lo} = memory_read(PC++)', 3,
        f'{hi} = memory_read(PC++)', 3,
    ]

def ld_hl_r(r: str) -> ConcreteSpecs:
    return [f'memory_write(HL, {r})', 3]

def ld_r_n(r: str) -> ConcreteSpecs:
    return [f'{r} = memory_read(PC++)', 3]

def out_c_r(r: str) -> ConcreteSpecs:
    return [f'io_write(BC, {r})', 4]


instructions: Table = {
    0x01: ('LD BC,nn', lambda: ld_dd_nn('BC')),
    0x04: ('INC B', [
        'B++',
        'F &= ~(FLAG_S | FLAG_Z | FLAG_H | FLAG_V | FLAG_N)',
        'F |= (B & 0x80) | (B == 0) << SHIFT_Z | (((B - 1) & 0x0F) + 1) & 0x10 | (B == 0x80) << SHIFT_V'
    ]),
    0x11: ('LD DE,nn',  lambda: ld_dd_nn('DE')),
    0x16: ('LD D,n',    lambda: ld_r_n('D')),
    0x21: ('LD HL,nn',  lambda: ld_dd_nn('HL')),
    0x3E: ('LD A,n',    lambda: ld_r_n('A')),
    0x75: ('LD (HL),L', lambda: ld_hl_r('L')),
    0xAF: ('XOR A', [
        'A = 0',
        'F &= ~(FLAG_S | FLAG_H | FLAG_N | FLAG_C)',
        'F |= FLAG_Z | FLAG_P'
    ]),
    0xC3: ('JP nn', [
        'Z  = memory_read(PC)',     3,
        'W  = memory_read(PC + 1)', 3,
        'PC = WZ'
    ]),
    0xE6: ('AND n', [
        'A &= memory_read(PC++)',
        'F &= ~(FLAG_S | FLAG_Z | FLAG_P | FLAG_N | FLAG_C)',
        'F |= (A & 0x80) | (A == 0) << SHIFT_Z | FLAG_H | parity[A]',
        3
    ]),
    0xED: {
        0x51: ('OUT (C),D', lambda: out_c_r('D')),
        0x78: ('IN A,(C)', [
            'A = io_read(BC)',
            'F &= ~(FLAG_S | FLAG_Z | FLAG_H | FLAG_V | FLAG_N)',
            'F |= (A & 0x80) | (A == 0) << SHIFT_Z | parity[A]',
            4
        ]),
        0x79: ('OUT (C),A', lambda: out_c_r('A')),
        0x91: ('NEXTREG reg,value', [
            'io_write(0x243B, memory_read(PC++))',
            'io_write(0x253B, memory_read(PC++))',
            8
        ]),
        0x92: ('NEXTREG reg,A', [
            'io_write(0x243B, memory_read(PC++))',
            'io_write(0x253B, A)',
            4
        ]),
        0xB0: ('LDIR', [
            'memory_write(DE++, memory_read(HL++))', 3 + 5,
            'F &= ~(FLAG_H | FLAG_V | FLAG_N)',
            'F |= (BC - 1 != 0) << SHIFT_V',
            'if (--BC) {',
            '  PC -= 2', 5,
            '}',
        ]),
    },
    0xF3: ('DI', 'IFF1 = IFF2 = 0')
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
                    terminator = ';' if concrete_spec[-1] not in ['{', '}'] else ''
                    f.write(f'{spaces}    {concrete_spec}{terminator}\n')
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
