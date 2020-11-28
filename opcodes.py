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
        f'HF  = HALFCARRY(A, {r}, A + {r})',
        f'VF  = SIGN(A) == SIGN({r})',
        f'A  += {r}',
        f'VF &= SIGN({r}) != SIGN(A)',
        f'SF  = SIGN(A)',
        f'ZF  = A == 0',
        f'NF  = 0',
        f'CF  = ((u16_t) A) + {r} > 255'
    ]

def and_r(r: str) -> ConcreteSpecs:
    return [
        f'A &= {r}',
        'SF = SIGN(A)',
        'ZF = A == 0',
        'HF = 1',
        'PF = parity[A]',
        'NF = CF = 0'
    ]

def dec_r(r: str) -> ConcreteSpecs:
    return [
        f'HF = HALFCARRY({r}, -1, {r} - 1)',
        f'VF = {r} == 0x80',
        f'{r}--',
        f'SF = SIGN({r})',
        f'ZF = {r} == 0',
        f'NF = 1'
    ]

def dec_ss(ss: str) -> ConcreteSpecs:
    return [f'{ss}--', 2]

def inc_r(r: str) -> ConcreteSpecs:
    return [
        f'HF = HALFCARRY({r}, 1, {r} + 1)',
        f'{r}++',
        f'SF = SIGN({r})',
        f'ZF = {r} == 0',
        f'VF = {r} == 0x80',
        f'NF = 0',
    ]

def inc_ss(ss: str) -> ConcreteSpecs:
    return [f'{ss}++', 2]

def jr_c_e(c: str) -> ConcreteSpecs:
    return [
        f'Z = memory_read(PC++)', 3,
        f'if ({c}) {{',
        f'  PC += (s8_t) Z', 5,
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

def ld_nn_dd(dd: str) -> ConcreteSpecs:
    hi, lo = dd
    return [
        f'Z = memory_read(PC++)',      3,
        f'W = memory_read(PC++)',      3,
        f'memory_write(WZ,     {lo})', 3,
        f'memory_write(WZ + 1, {hi})', 3,
    ]

def ld_r_hl(r: str) -> ConcreteSpecs:
    return [f'{r} = memory_read(HL)', 3]

def ld_r_n(r: str) -> ConcreteSpecs:
    return [f'{r} = memory_read(PC++)', 3]

def ld_r_r(r1: str, r2: str) -> ConcreteSpecs:
    return f'{r1} = {r2}'

def out_c_r(r: str) -> ConcreteSpecs:
    return [f'io_write(BC, {r})', 4]

def push_qq(qq: str) -> ConcreteSpecs:
    hi, lo = qq
    return [
        1,
        f'memory_write(--SP, {hi})', 3,
        f'memory_write(--SP, {lo})', 3,
    ]

def rlc_r(r: str) -> ConcreteSpecs:
    return [
        f'CF  = {r} >> 7',
        f'{r} = {r} << 1 | CF',
        f'SF = SIGN({r})',
        f'ZF = {r} == 0',
        f'HF = 0',
        f'PF = parity[{r}]',
        f'NF = 0',
    ]

def rst(address: int) -> ConcreteSpecs:
    return [
        1,
        f'memory_write(--SP, PCH)', 3,
        f'memory_write(--SP, PCL)',
        f'PC = 0x{address:02X}',    3,
    ]

def sbc_hl_ss(ss: str) -> ConcreteSpecs:
    hi, _ = ss
    return [
        f'WZ  = HL',
        f'HL -= {ss} + CF',
        4,
        f'SF  = SIGN(H)',
        f'ZF  = HL == 0',
        f'HF  = HALFCARRY16(WZ, -({ss} + CF), HL)',
        f'VF  = SIGN(W) == SIGN({hi}) && SIGN({hi}) != SIGN(H)',
        f'NF  = 1',
        f'CF  = HL < {ss}',
        3,
    ]

def srl_r(r: str) -> ConcreteSpecs:
    return [
        f'CF = {r} & 1',
        f'{r} >>= 1',
        f'SF = HF = NF = 0',
        f'ZF = {r} == 0',
        f'PF = parity[{r}]',
    ]


instructions: Table = {
    0x01: ('LD BC,nn',  lambda: ld_dd_nn('BC')),
    0x03: ('INC BC',    lambda: inc_ss('BC')),
    0x04: ('INC B',     lambda: inc_r('B')),
    0x06: ('LD B,n',    lambda: ld_r_n('B')),
    0x08: ("EX AF,AF'", [
        'cpu_flags_pack()',
        'cpu_exchange(&AF, &AF_)',
        'cpu_flags_unpack()'
    ]),
    0x0A: ('DEC BC',    lambda: dec_ss('BC')),
    0x0C: ('INC C',     lambda: inc_r('C')),
    0x10: ('DJNZ e', [
        1,
        'Z = memory_read(PC++)', 3,
        'if (--B) {',
        '  PC += (s8_t) Z', 5,
        '}',
    ]),
    0x11: ('LD DE,nn',  lambda: ld_dd_nn('DE')),
    0x16: ('LD D,n',    lambda: ld_r_n('D')),
    0x18: ('JR e',      [
        'Z = memory_read(PC++)', 3,
        'PC += (s8_t) Z', 5,
    ]),
    0x1D: ('DEC E',     lambda: dec_r('E')),
    0x20: ('JR NZ,e',   lambda: jr_c_e('!ZF')),
    0x21: ('LD HL,nn',  lambda: ld_dd_nn('HL')),
    0x23: ('INC HL',    lambda: inc_ss('HL')),
    0x28: ('JR Z,e',    lambda: jr_c_e('ZF')),
    0x2B: ('DEC HL',    lambda: dec_ss('HL')),
    0x2F: ('CPL', [
        'A = ~A',
        'HF = NF = 1',
    ]),
    0x30: ('JR NC,e',   lambda: jr_c_e('!CF')),
    0x31: ('LD SP,nn',  lambda: ld_dd_nn('SP')),
    0x32: ('LD nn,A',   [
        'Z = memory_read(PC++)', 3,
        'W = memory_read(PC++)', 3,
        'memory_write(WZ, A)',   3,
    ]),
    0x36: ('LD (HL),n', [
        f'Z = memory_read(PC++)', 3,
        f'memory_write(HL, Z)',   3,
    ]),
    0x38: ('JR C,e',    lambda: jr_c_e('C')),
    0x3C: ('INC A',     lambda: inc_r('A')),
    0x3D: ('DEC A',     lambda: dec_r('A')),
    0x3E: ('LD A,n',    lambda: ld_r_n('A')),
    0x46: ('LD B,(HL)', lambda: ld_r_hl('B')),
    0x4B: ('LD C,E',    lambda: ld_r_r('C', 'B')),
    0x4E: ('LD C,(HL)', lambda: ld_r_hl('C')),
    0x6F: ('LD L,A',    lambda: ld_r_r('L', 'A')),
    0x75: ('LD (HL),L', lambda: ld_hl_r('L')),
    0x77: ('LD (HL),A', lambda: ld_hl_r('A')),
    0x79: ('LD A,C',    lambda: ld_r_r('A', 'C')),
    0x7A: ('LD A,D',    lambda: ld_r_r('A', 'D')),
    0x7C: ('LD A,H',    lambda: ld_r_r('A', 'H')),
    0x7E: ('LD A,(HL)', lambda: ld_r_hl('A')),
    0x80: ('ADD A,B',   lambda: add_a_r('B')),
    0x87: ('ADD A,A',   lambda: add_a_r('A')),
    0xA2: ('AND D',     lambda: and_r('D')),
    0xA7: ('AND A',     lambda: and_r('A')),
    0xAF: ('XOR A', [
        'A = 0',
        'SF = HF = NF = CF = 0',
        'ZF = PF = 1',
    ]),
    0xC3: ('JP nn', [
        'Z  = memory_read(PC)',     3,
        'W  = memory_read(PC + 1)', 3,
        'PC = WZ'
    ]),
    0xC5: ('PUSH BC', lambda: push_qq('BC')),
    0xC9: ('RET', [
        f'PCL = memory_read(SP++)', 3,
        f'PCH = memory_read(SP++)', 3,
    ]),
    0xD9: ('EXX', [
        'cpu_exchange(&BC, &BC_)',
        'cpu_exchange(&DE, &DE_)',
        'cpu_exchange(&HL, &HL_)',
    ]),
    0xDD: {
        0x6F: ('LD IXL,A', lambda: ld_r_r('IXL', 'A')),
        0x7D: ('LD A,IXL', lambda: ld_r_r('A', 'IXL')),
    },
    0xE3: ('EX (SP),HL', [
        f'Z = memory_read(SP)',     3,
        f'memory_write(SP,     L)', 4,
        f'W = memory_read(SP + 1)', 3,
        f'memory_write(SP + 1, H)', 5,
        f'H = W',
        f'L = Z',
    ]),
    0xE6: ('AND n', [
        'A &= memory_read(PC++)',
        'SF = SIGN(A)',
        'ZF = A == 0',
        'HF = 1',
        'PF = parity[A]',
        'NF = CF = 0',
        3
    ]),
    0xE7: ('RST 20h', lambda: rst(0x20)),
    0xCB: {
        0x02: ('RLC D', lambda: rlc_r('D')),
        0x3F: ('SRL A', lambda: srl_r('A')),
    },
    0xED: {
        0x43: ('LD (nn),BC', lambda: ld_nn_dd('BC')),
        0x4B: ('LD BC,(nn)', lambda: ld_dd_nn('BC')),
        0x51: ('OUT (C),D',  lambda: out_c_r('D')),
        0x52: ('SBC HL,DE',  lambda: sbc_hl_ss('DE')),
        0x78: ('IN A,(C)', [
            'A  = io_read(BC)',
            'SF = SIGN(A)',
            'ZF = A == 0',
            'HF = NF = 0',
            'PF = parity[A]',
            4
        ]),
        0x79: ('OUT (C),A', lambda: out_c_r('A')),
        0x8A: ('PUSH nn', [
            f'Z = memory_read(PC++)',
            f'W = memory_read(PC++)',
            f'memory_write(--SP, W)',
            f'memory_write(--SP, Z)',
            11
        ]),
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
            'Z  = memory_read(HL++)', 3,
            'memory_write(DE++, Z)',
            'HF = NF = 0',
            'VF = BC - 1 != 0',
            5,
            'if (--BC) {',
            '  PC -= 2', 5,
            '}',
        ]),
        0xB8: ('LDDR', [
            'Z = memory_read(HL--)', 3,
            'memory_write(DE--, Z)',
            'HF = NF = 0',
            'VF = BC - 1 != 0',
            5,
            'if (--BC) {',
            '  PC -= 2', 5,
            '}',
        ])
    },
    0xF3: ('DI', 'IFF1 = IFF2 = 0'),
    0xFE: ('CP n', [
        f'Z  = memory_read(PC++)',
        f'SF = SIGN(A - Z)',
        f'ZF = A - Z == 0',
        f'HF = HALFCARRY(A, Z, A - Z)',
        f'VF = SIGN(A) == SIGN(Z) && SIGN(Z) != SIGN(A - Z)',
        f'NF = 1',
        f'CF = A < Z',
        3,
    ]),
}


def generate(instructions: Table, f: io.TextIOBase, level: int = 0, prefix: Optional[List[Opcode]] = None) -> None:
    prefix = prefix or []
    spaces = ' ' * level * 4

    f.write(f'''{spaces}opcode = memory_read(PC++);
{spaces}clock_ticks(4);
{spaces}switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, specifications = item
            f.write(f'{spaces}  case 0x{opcode:02X}:  /* {mnemonic} */\n')
            f.write(f'{spaces}    fprintf(stderr, "%04Xh {mnemonic:20s} A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\\n", PC - 1 - {len(prefix)}, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");\n')

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
{spaces}    fprintf(stderr, "Unknown opcode {s}%02Xh at %04Xh\\n", opcode, PC - {n});
{spaces}    return -1;
{spaces}}}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
