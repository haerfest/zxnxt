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


def add_a_r(r: str) -> ConcreteSpecs:
    return [
        f'FH  = HALFCARRY(A, {r}, A + {r})',
        f'FV  = SIGN(A) == SIGN({r})',
        f'A  += {r}',
        f'FV &= SIGN({r}) != SIGN(A)',
        f'FS  = SIGN(A)',
        f'FZ  = A == 0',
        f'FN  = 0',
        f'FC  = ((u16_t) A) + {r} > 255'
    ]

def and_r(r: str) -> ConcreteSpecs:
    return [
        f'A &= {r}',
        'FS = SIGN(A)',
        'FZ = A == 0',
        'FH = 1',
        'FP = parity[A]',
        'FN = FC = 0'
    ]

def dec_ss(ss: str) -> ConcreteSpecs:
    return [f'{ss}--', 2]

def inc_r(r: str) -> ConcreteSpecs:
    return [
        f'FH = HALFCARRY({r}, 1, {r} + 1)',
        f'{r}++',
        f'FS = SIGN({r})',
        f'FZ = {r} == 0',
        f'FV = {r} = 0x80',
        f'FN = 0',
    ]

def inc_ss(ss: str) -> ConcreteSpecs:
    return [f'{ss}++', 2]

def jr_c_e(c: str) -> ConcreteSpecs:
    return [
        f'tmp8 = memory_read(PC++)', 3,
        f'if ({c}) {{',
        f'  PC += (s8_t) tmp8', 5,
        f'}}'
    ]

def ld_dd_nn(dd: str) -> ConcreteSpecs:
    hi, lo = dd
    return [
        f'{lo} = memory_read(PC++)', 3,
        f'{hi} = memory_read(PC++)', 3,
    ]

def ld_hl_r(r: str) -> ConcreteSpecs:
    return [f'memory_write(HL, {r})', 3]

def ld_r_hl(r: str) -> ConcreteSpecs:
    return [f'{r} = memory_read(HL)', 3]

def ld_r_n(r: str) -> ConcreteSpecs:
    return [f'{r} = memory_read(PC++)', 3]

def ld_r_r(r1: str, r2: str) -> ConcreteSpecs:
    return f'{r1} = {r2}'

def out_c_r(r: str) -> ConcreteSpecs:
    return [f'io_write(BC, {r})', 4]

def srl_r(r: str) -> ConcreteSpecs:
    return [
        f'FC = {r} & 1',
        f'{r} >>= 1',
        f'FS = FH = FN = 0',
        f'FZ = {r} == 0',
        f'FP = parity[{r}]',
    ]


instructions: Table = {
    0x01: ('LD BC,nn',  lambda: ld_dd_nn('BC')),
    0x03: ('INC BC',    lambda: inc_ss('BC')),
    0x04: ('INC B',     lambda: inc_r('B')),
    0x08: ("EX AF,AF'", [
        'cpu_flags_pack()',
        'cpu_exchange(&AF, &AF_)',
        'cpu_flags_unpack()'
    ]),
    0x0A: ('DEC BC',    lambda: dec_ss('BC')),
    0x0C: ('INC C',     lambda: inc_r('C')),
    0x10: ('DJNZ e', [
        1,
        'tmp8 = memory_read(PC++)', 3,
        'if (--B) {',
        '  PC += (s8_t) tmp8', 5,
        '}',
    ]),
    0x11: ('LD DE,nn',  lambda: ld_dd_nn('DE')),
    0x16: ('LD D,n',    lambda: ld_r_n('D')),
    0x18: ('JR e',      [
        'tmp8 = memory_read(PC++)', 3,
        'PC += (s8_t) tmp8', 5,
    ]),
    0x20: ('JR NZ,e',   lambda: jr_c_e('!FZ')),
    0x21: ('LD HL,nn',  lambda: ld_dd_nn('HL')),
    0x23: ('INC HL',    lambda: inc_ss('HL')),
    0x28: ('JR Z,e',    lambda: jr_c_e('FZ')),
    0x2B: ('DEC HL',    lambda: dec_ss('HL')),
    0x2F: ('CPL', [
        'A = ~A',
        'FH = FN = 1',
    ]),
    0x30: ('JR NC,e',   lambda: jr_c_e('!FC')),
    0x36: ('LD (HL),n', [
        f'tmp8 = memory_read(PC++)', 3,
        f'memory_write(HL, tmp8)',   3,
    ]),
    0x3C: ('INC A',     lambda: inc_r('A')),
    0x3E: ('LD A,n',    lambda: ld_r_n('A')),
    0x6F: ('LD L,A',    lambda: ld_r_r('L', 'A')),
    0x75: ('LD (HL),L', lambda: ld_hl_r('L')),
    0x77: ('LD (HL),A', lambda: ld_hl_r('A')),
    0x79: ('LD A,C',    lambda: ld_r_r('A', 'C')),
    0x7A: ('LD A,D',    lambda: ld_r_r('A', 'D')),
    0x7E: ('LD A,(HL)', lambda: ld_r_hl('A')),
    0x87: ('ADD A,A',   lambda: add_a_r('A')),
    0xA2: ('AND D',     lambda: and_r('D')),
    0xAF: ('XOR A', [
        'A = 0',
        'FS = FH = FN = FC = 0',
        'FZ = FP = 1',
    ]),
    0xC3: ('JP nn', [
        'Z  = memory_read(PC)',     3,
        'W  = memory_read(PC + 1)', 3,
        'PC = WZ'
    ]),
    0xD9: ('EXX', [
        'cpu_exchange(&BC, &BC_)',
        'cpu_exchange(&DE, &DE_)',
        'cpu_exchange(&HL, &HL_)',
    ]),
    0xDD: {
        0x6F: ('LD IXL,A', lambda: ld_r_r('IXL', 'A')),
    },
    0xE6: ('AND n', [
        'A &= memory_read(PC++)',
        'FS = SIGN(A)',
        'FZ = A == 0',
        'FH = 1',
        'FP = parity[A]',
        'FN = FC = 0',
        3
    ]),
    0xCB: {
        0x3F: ('SRL A', lambda: srl_r('A')),
    },
    0xED: {
        0x51: ('OUT (C),D', lambda: out_c_r('D')),
        0x78: ('IN A,(C)', [
            'A = io_read(BC)',
            'FS = SIGN(A)',
            'FZ = A == 0',
            'FH = FN = 0',
            'FP = parity[A]',
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
            'tmp8 = memory_read(HL++)', 3,
            'memory_write(DE++, tmp8)',
            'FH = FN = 0',
            'FV = BC - 1 != 0',
            5,
            'if (--BC) {',
            '  PC -= 2', 5,
            '}',
        ]),
    },
    0xF3: ('DI', 'IFF1 = IFF2 = 0'),
    0xFE: ('CP n', [
        f'tmp8 = memory_read(PC++)',
        f'FS   = SIGN(A - tmp8)',
        f'FZ   = A - tmp8 == 0',
        f'FH   = HALFCARRY(A, tmp8, A - tmp8)',
        f'FV   = SIGN(A) == SIGN(tmp8) && SIGN(tmp8) != SIGN(A - tmp8)',
        f'FN   = 1',
        f'FC   = A < tmp8',
        3,
    ]),
}


def generate(instructions: Table, f: io.TextIOBase, level: int = 0, prefix: Optional[List[Opcode]] = None) -> None:
    prefix = prefix or []
    spaces = ' ' * level * 4

    if level == 0:
        f.write(f'{spaces}printf("%04Xh\\n", PC);\n')

    f.write(f'''{spaces}opcode = memory_read(PC++);
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
                    concrete_spec = specification()
                    if isinstance(concrete_spec, list):
                        concrete_specs.extend(concrete_spec)
                    else:
                        concrete_specs.append(concrete_spec)
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
{spaces}    fprintf(stderr, "Unknown opcode {s}%02Xh at %04Xh\\n", opcode, cpu.pc - {n});
{spaces}    return -1;
{spaces}}}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
