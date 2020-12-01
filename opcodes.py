#!/usr/bin/env python3


import io
from typing import *


Opcode      = int
Mnemonic    = str
C           = Optional[str]
CGenerator  = Callable[[], C]
Instruction = Tuple[Mnemonic, Union[C, CGenerator]]
Table       = Dict[Opcode, Union[Instruction, 'Table']]


def adc_a_r(r: str) -> C:
    return None
    
def add_a_r(r: str) -> C:
    return None

def and_r(r: str) -> C:
    return f'''
        A &= memory_read(PC++); T(3);
        F = SZ53P(A) | (1 << HF_SHIFT);
    '''

def dec_r(r: str) -> C:
    return ''

def dec_ss(ss: str) -> C:
    return f'{ss}--; T(2);'

def inc_r(r: str, delta: int = 1) -> C:
    return ''

def inc_ss(ss: str) -> C:
    return f'{ss}++; T(2);'

def jr_c_e(c: str) -> C:
    return f'''
        Z = memory_read(PC++); T(3);
        if ({c}) {{
          PC += (s8_t) Z; T(5);
        }}
    '''

def ld_dd_nn(dd: str) -> C:
    hi, lo = dd
    return f'''
        {lo} = memory_read(PC++); T(3);
        {hi} = memory_read(PC++); T(3);
    '''

def ld_hl_r(r: str) -> C:
    return f'memory_write(HL, {r}); T(3);'

def ld_nn_dd(dd: str) -> C:
    hi, lo = dd
    return f'''
        Z = memory_read(PC++);      T(3);
        W = memory_read(PC++);      T(3);
        memory_write(WZ,     {lo}); T(3);
        memory_write(WZ + 1, {hi}); T(3);
    '''

def ld_r_hl(r: str) -> C:
    return f'{r} = memory_read(HL); T(3);'

def ld_r_n(r: str) -> C:
    return f'{r} = memory_read(PC++); T(3);'

def ld_r_r(r1: str, r2: str) -> C:
    return f'{r1} = {r2};'


def ldxr(op: str) -> C:
    return f'''
        Z  = memory_read(HL{op}{op}); T(3);
        memory_write(DE{op}{op}, Z);
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (BC - 1 != 0) << VF_SHIFT;
        T(5);
        if (--BC) {{ PC -= 2; T(5); }}
    '''

def out_c_r(r: str) -> C:
    return f'io_write(BC, {r}); T(4);'

def push_qq(qq: str) -> C:
    hi, lo = qq
    return f'''
        T(1);
        memory_write(--SP, {hi}); T(3);
        memory_write(--SP, {lo}); T(3);
    '''

def rlc_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} = {r} << 1 | carry;
        F = SZ53P({r}) | carry;
    '''

def rst(address: int) -> C:
    return f'''
        T(1);
        memory_write(--SP, PCH); T(3);
        memory_write(--SP, PCL);
        PC = 0x{address:02X};    T(3);
    '''

def sbc_hl_ss(ss: str) -> C:
    return None

def srl_r(r: str) -> C:
    return f'''
        const u8_t previous = {r};
        {r} >>= 1;
        F = SZ53P({r}) | (previous & 0x01);
    '''


instructions: Table = {
    0x01: ('LD BC,nn',  lambda: ld_dd_nn('BC')),
    0x03: ('INC BC',    lambda: inc_ss('BC')),
    0x04: ('INC B',     lambda: inc_r('B')),
    0x06: ('LD B,n',    lambda: ld_r_n('B')),
    0x08: ("EX AF,AF'",
           '''
           const u16_t tmp = AF;
           AF = AF_;
           AF_ = tmp;
           '''),
    0x0A: ('DEC BC',    lambda: dec_ss('BC')),
    0x0C: ('INC C',     lambda: inc_r('C')),
    0x10: ('DJNZ e',
           f'''
           T(1);
           Z = memory_read(PC++); T(3);
           if (--B) {{
             PC += (s8_t) Z; T(5);
           }}
           '''),
    0x11: ('LD DE,nn',  lambda: ld_dd_nn('DE')),
    0x16: ('LD D,n',    lambda: ld_r_n('D')),
    0x18: ('JR e',
           '''
           Z = memory_read(PC++); T(3);
           PC += (s8_t) Z;        T(5);
           '''),
    0x1D: ('DEC E',     lambda: dec_r('E')),
    0x20: ('JR NZ,e',   lambda: jr_c_e('!ZF')),
    0x21: ('LD HL,nn',  lambda: ld_dd_nn('HL')),
    0x23: ('INC HL',    lambda: inc_ss('HL')),
    0x28: ('JR Z,e',    lambda: jr_c_e('ZF')),
    0x2B: ('DEC HL',    lambda: dec_ss('HL')),
    0x2F: ('CPL',
           '''
           A = ~A;
           F |= (1 << HF_SHIFT) | (1 << NF_SHIFT);
           '''),
    0x30: ('JR NC,e',   lambda: jr_c_e('!CF')),
    0x31: ('LD SP,nn',  lambda: ld_dd_nn('SP')),
    0x32: ('LD nn,A',
           '''
           Z = memory_read(PC++); T(3);
           W = memory_read(PC++); T(3);
           memory_write(WZ, A);   T(3);
           '''),
    0x36: ('LD (HL),n',
           '''
           Z = memory_read(PC++); T(3);
           memory_write(HL, Z);   T(3);
           '''),
    0x38: ('JR C,e',    lambda: jr_c_e('CF')),
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
    0xAF: ('XOR A',
           '''
           A = 0;
           F = SZ53P(0);
           '''),
    0xC3: ('JP nn',
           '''
           Z  = memory_read(PC);     T(3);
           W  = memory_read(PC + 1); T(3);
           PC = WZ;
           '''),
    0xC5: ('PUSH BC', lambda: push_qq('BC')),
    0xC9: ('RET',
           '''
           PCL = memory_read(SP++); T(3);
           PCH = memory_read(SP++); T(3);
           '''),
    0xD9: ('EXX',
           '''
           u16_t tmp;
           tmp = BC; BC = BC_; BC_ = tmp;
           tmp = DE; DE = DE_; DE_ = tmp;
           tmp = HL; HL = HL_; HL_ = tmp;
           '''),
    0xDD: {
        0x6F: ('LD IXL,A', lambda: ld_r_r('IXL', 'A')),
        0x7D: ('LD A,IXL', lambda: ld_r_r('A', 'IXL')),
    },
    0xE3: ('EX (SP),HL',
           '''
           Z = memory_read(SP);     T(3);
           W = memory_read(SP + 1); T(3);
           memory_write(SP,     L); T(4);
           memory_write(SP + 1, H); T(5);
           H = W;
           L = Z;
           '''),
    0xE6: ('AND n',
           '''
           A &= memory_read(PC++); T(3);
           F = SZ53P(A) | (1 << HF_SHIFT);
           '''),
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
        0x78: ('IN A,(C)',
               f'''
               A = io_read(BC); T(4);
               F = SZ53P(A);
               '''),
        0x79: ('OUT (C),A', lambda: out_c_r('A')),
        0x8A: ('PUSH nn',
               '''
               Z = memory_read(PC++);
               W = memory_read(PC++);
               memory_write(--SP, W);
               memory_write(--SP, Z);
               T(11);
               '''),
        0x91: ('NEXTREG reg,value',
               '''
               io_write(0x243B, memory_read(PC++));
               io_write(0x253B, memory_read(PC++));
               T(8);
               '''),
        0x92: ('NEXTREG reg,A',
               '''
               io_write(0x243B, memory_read(PC++));
               io_write(0x253B, A);
               T(4);
               '''),
        0xB0: ('LDIR', lambda: ldxr('+')),
        0xB8: ('LDDR', lambda: ldxr('-')),
    },
    0xF3: ('DI', 'IFF1 = IFF2 = 0;'),
    0xFE: ('CP n',
           '''
           u8_t result;
           Z      = memory_read(PC++);
           result = A - Z;
           F      = SZ53(result) | HF_SUB(A, Z, result) | VF_SUB(A, Z, result) | NF_MASK | (A < Z) << CF_SHIFT;
           T(3);
           ''')
}


def generate(instructions: Table, f: io.TextIOBase, prefix: Optional[List[Opcode]] = None) -> None:
    prefix = prefix or []

    f.write(f'''
opcode = memory_read(PC++);
T(4);
switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, spec = item
            c = spec() if callable(spec) else spec
            if c:
                f.write(f'''
case 0x{opcode:02X}:  /* {mnemonic} */
  fprintf(stderr, "%04Xh {mnemonic:20s} A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\\n", PC - 1 - {len(prefix)}, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
  {{
    {c}
  }}
  break;

''')

        else:
            f.write(f'case 0x{opcode:02X}:\n')
            generate(item, f, prefix + [opcode])

    s = ''.join(f'{opcode:02X}h ' for opcode in prefix)
    n = len(prefix) + 1
    f.write(f'''
default:
  fprintf(stderr, "Unknown opcode {s}%02Xh at %04Xh\\n", opcode, PC - {n});
  return -1;
}}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
