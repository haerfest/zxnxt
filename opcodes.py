#!/usr/bin/env python3


import io
from typing import *


Opcode      = int
Mnemonic    = str
C           = Optional[str]
CGenerator  = Callable[[], C]
ReadAhead   = bool
Instruction = Tuple[Mnemonic, Union[C, CGenerator]]
Table       = Dict[Opcode, Union[Instruction, 'Table']]


def adc_a_r(r: str) -> C:
    return None
    
def add_a_r(r: str) -> C:
    return f'''
        const u8_t index = LOOKUP_IDX(A, {r}, A + {r});
        const u8_t carry = A > 0xFF - {r};
        A += {r};
        F = SZ53(A) | HF_ADD_IDX(index) | VF_ADD_IDX(index) | carry;
    '''

def add_hl_ss(ss: str) -> C:
    return f'''
        const u16_t prev  = HL;
        const u8_t  carry = HL > 0xFFFF - {ss};
        HL += {ss}; T(4 + 3);
        F &= ~(HF_MASK | NF_MASK | CF_MASK);
        F |= HF_ADD(prev >> 8, {ss} >> 8, HL >> 8) | carry;
    '''

def and_r(r: str) -> C:
    return f'''
        A &= memory_read(PC++); T(3);
        F = SZ53P(A) | (1 << HF_SHIFT);
    '''

def bit_b_xy_d(b: int, xy: str) -> C:
    return f'''
        T(1);
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        PC++;
        F &= ~(ZF_MASK | NF_MASK);
        F |= (memory_read(WZ) & 1 << {b} ? 0 : ZF_MASK) | HF_MASK;
        T(4);
    '''

def call(cond: Optional[str] = None) -> C:
    s = '''
        Z = memory_read(PC++);   T(3);
        W = memory_read(PC++);   T(4);
    '''
    
    if cond:
        s += f'if ({cond}) {{\n'

    s += '''
        memory_write(--SP, PCH); T(3);
        memory_write(--SP, PCL); T(3);
        PC = WZ;
    '''

    if cond:
        s += '}\n'

    return s

def cp_r(r: str) -> C:
    return f'''
        const u8_t result = A - {r};
        const u8_t idx    = LOOKUP_IDX(A, {r}, result);
        F                 = SZ53(result) | HF_SUB_IDX(idx) | VF_SUB_IDX(idx) | NF_MASK | (A < {r}) << CF_SHIFT;
    '''

def cp_xy_d(xy: str) -> C:
    return f'''
        u8_t tmp;
        u8_t result;
        u8_t idx;
        WZ     = {xy} + (s8_t) memory_read(PC++); T(3);
        tmp    = memory_read(WZ);                 T(5);
        result = A - tmp;
        idx    = LOOKUP_IDX(A, tmp, result);
        F      = SZ53(result) | HF_SUB_IDX(idx) | VF_SUB_IDX(idx) | NF_MASK | (A < tmp) << CF_SHIFT;
    '''

def dec_r(r: str) -> C:
    return f'''
        {r}--;
        F = SZ53({r}) | HF_SUB({r} + 1, 1, {r}) | ({r} == 0x79) << VF_SHIFT | NF_MASK;
    '''

def dec_ss(ss: str) -> C:
    return f'{ss}--; T(2);'

def dec_xy_d(xy: str) -> C:
    return f'''
        u8_t m;
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        m = memory_read(WZ) - 1;              T(5);
        memory_write(WZ, m);                  T(4);
        F = SZ53(m) | HF_SUB(m + 1, 1, m) | (m == 0x79) << VF_SHIFT | NF_MASK;
        T(3);
    '''

def ex(r1: str, r2: str) -> C:
    return f'''
        const u16_t tmp = {r1};
        {r1} = {r2};
        {r2} = tmp;
    '''

def inc_r(r: str) -> C:
    return f'''
       const u8_t index = LOOKUP_IDX({r}, 1, ++{r});
       F = SZ53({r}) | HF_ADD_IDX(index) | VF_ADD_IDX(index);
    '''

def inc_ss(ss: str) -> C:
    return f'{ss}++; T(2);'

def inx(op: str) -> C:
    return f'''
        u8_t tmp;
        WZ = B << 8 | C;
        tmp = io_read(WZ);     T(5);
        memory_write(HL, tmp); T(3);
        B--;
        HL{op}{op};            T(4);
        F &= ~ZF_MASK;
        F |= (B == 0) << ZF_SHIFT | NF_MASK;
    '''

def jr_c_e(c: str) -> C:
    return f'''
        Z = memory_read(PC++); T(3);
        if ({c}) {{
          PC += (s8_t) Z; T(5);
        }}
    '''

def jp(cond: Optional[str] = None) -> C:
    s = '''
        Z = memory_read(PC++); T(3);
        W = memory_read(PC++); T(3);
    '''

    if cond:
        s += f'if ({cond}) {{\n'

    s += 'PC = WZ;\n'

    if cond:
        s += '}'

    return s

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

def ld_r_xy_d(r: str, xy: str) -> C:
    return f'''
        WZ = {xy} + (s8_t) memory_read(PC++); T(3 + 5);
        {r} = memory_read(WZ); T(3);
    '''

def ld_xy_d_r(xy: str, r: str) -> C:
    return f'''
        WZ = {xy} + (s8_t) memory_read(PC++); T(3 + 5);
        memory_write(WZ, {r}); T(3);
    '''
    
def ldxr(op: str) -> C:
    return f'''
        Z  = memory_read(HL{op}{op});
        T(3);
        memory_write(DE{op}{op}, Z);
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (BC - 1 != 0) << VF_SHIFT;
        T(5);
        if (--BC) {{ PC -= 2; T(5); }}
    '''

def or_r(r: str) -> C:
    return f'''
        A |= {r};
        F = SZ53P(A);
    '''

def out_c_r(r: str) -> C:
    return f'io_write(BC, {r}); T(4);'

def pop_qq(qq: str) -> C:
    hi, lo = qq
    return f'''
        {lo} = memory_read(SP++); T(3);
        {hi} = memory_read(SP++); T(3);
    '''

def push_qq(qq: str) -> C:
    hi, lo = qq
    return f'''
        T(1);
        memory_write(--SP, {hi}); T(3);
        memory_write(--SP, {lo}); T(3);
    '''

def ret(cond: Optional[str] = None) -> C:
    s = 'T(1);\n'
    if cond:
        s += f'if ({cond}) {{\n';

    s += '''
        PCL = memory_read(SP++); T(3);
        PCH = memory_read(SP++); T(3);
    '''

    if cond:
        s += '}\n'

    return s

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

def sub_r(r: str) -> C:
    return f'''
        const u8_t previous = A;
        const u8_t idx      = LOOKUP_IDX(A, {r}, A - {r});
        A -= {r};
        F = SZ53(A) | HF_SUB_IDX(idx) | VF_SUB_IDX(idx) | NF_MASK | (previous < {r});
    '''


instructions: Table = {
    0x00: ('NOP',       ''),
    0x01: ('LD BC,nn',  lambda: ld_dd_nn('BC')),
    0x03: ('INC BC',    lambda: inc_ss('BC')),
    0x04: ('INC B',     lambda: inc_r('B')),
    0x05: ('DEC B',     lambda: dec_r('B')),
    0x06: ('LD B,n',    lambda: ld_r_n('B')),
    0x08: ("EX AF,AF'", lambda: ex('AF', 'AF_')),
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
    0x14: ('INC D',     lambda: inc_r('D')),
    0x16: ('LD D,n',    lambda: ld_r_n('D')),
    0x17: ('RLA',
           '''
           const u8_t carry = F & CF_MASK;
           F &= ~(HF_MASK | NF_MASK | CF_MASK);
           F |= (A & 0x80) >> 7;
           A <<= 1 | carry;
           '''),
    0x18: ('JR e',
           '''
           Z = memory_read(PC++); T(3);
           PC += (s8_t) Z;        T(5);
           '''),
    0x1D: ('DEC E',      lambda: dec_r('E')),
    0x1E: ('LD E,n',     lambda: ld_r_n('E')),
    0x20: ('JR NZ,e',    lambda: jr_c_e('!ZF')),
    0x21: ('LD HL,nn',   lambda: ld_dd_nn('HL')),
    0x23: ('INC HL',     lambda: inc_ss('HL')),
    0x28: ('JR Z,e',     lambda: jr_c_e('ZF')),
    0x29: ('ADD HL,HL',  lambda: add_hl_ss('HL')),
    0x2A: ('LD HL,(nn)',
           '''
           Z = memory_read(PC++);   T(3);
           W = memory_read(PC++);   T(3);
           L = memory_read(WZ);     T(3);
           H = memory_read(WZ + 1); T(3);
           '''),
    0x2B: ('DEC HL',     lambda: dec_ss('HL')),
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
    0x3A: ('LD A,(nn)',
           '''
           Z = memory_read(PC++); T(3);
           W = memory_read(PC++); T(3);
           A = memory_read(WZ);   T(3);
           '''),
    0x3C: ('INC A',     lambda: inc_r('A')),
    0x3D: ('DEC A',     lambda: dec_r('A')),
    0x3E: ('LD A,n',    lambda: ld_r_n('A')),
    0x46: ('LD B,(HL)', lambda: ld_r_hl('B')),
    0x47: ('LD B,A',    lambda: ld_r_r('B', 'A')),
    0x4B: ('LD C,E',    lambda: ld_r_r('C', 'B')),
    0x4E: ('LD C,(HL)', lambda: ld_r_hl('C')),
    0x52: ('LD D,D',    lambda: ld_r_r('D', 'D')),
    0x67: ('LD H,A',    lambda: ld_r_r('H', 'A')),
    0x6F: ('LD L,A',    lambda: ld_r_r('L', 'A')),
    0x75: ('LD (HL),L', lambda: ld_hl_r('L')),
    0x77: ('LD (HL),A', lambda: ld_hl_r('A')),
    0x79: ('LD A,C',    lambda: ld_r_r('A', 'C')),
    0x7A: ('LD A,D',    lambda: ld_r_r('A', 'D')),
    0x7C: ('LD A,H',    lambda: ld_r_r('A', 'H')),
    0x7E: ('LD A,(HL)', lambda: ld_r_hl('A')),
    0x80: ('ADD A,B',   lambda: add_a_r('B')),
    0x87: ('ADD A,A',   lambda: add_a_r('A')),
    0x93: ('SUB E',     lambda: sub_r('E')),
    0xA2: ('AND D',     lambda: and_r('D')),
    0xA7: ('AND A',     lambda: and_r('A')),
    0xAF: ('XOR A',
           '''
           A = 0;
           F = SZ53P(0);
           '''),
    0xB3: ('OR E',      lambda: or_r('E')),
    0xB9: ('CP C',      lambda: cp_r('C')),
    0xC2: ('JP NZ,nn',  lambda: jp('!(F & ZF_MASK)')),
    0xC3: ('JP nn',     jp),
    0xC4: ('CALL NZ,nn', lambda: call('!(F & ZF_MASK)')),
    0xC5: ('PUSH BC',    lambda: push_qq('BC')),
    0xC6: ('ADD A,n',
           '''
           const u8_t n     = memory_read(PC++); T(3);
           const u8_t index = LOOKUP_IDX(A, n, A + n);
           const u8_t carry = A > 255 - n;
           A += n;
           F = SZ53(A) | HF_ADD_IDX(index) | VF_ADD_IDX(index) | carry;
           '''),
    0xC8: ('RET Z',      lambda: ret('F & ZF_MASK')),
    0xC9: ('RET',        ret),
    0xCD: ('CALL nn',    call),
    0xD3: ('OUT (n),A',
           '''
           WZ = A << 8 | memory_read(PC++); T(3);
           io_write(WZ, A);                 T(4);
           '''),
    0xD4: ('CALL NC,nn', lambda: call('!(F & CF_MASK)')),
    0xD6: ('SUB n',
           '''
           const u8_t previous = A;
           u8_t idx;
           Z   = memory_read(PC++); T(3);
           A  -= Z;
           idx = LOOKUP_IDX(previous, Z, A);
           F   = SZ53(A) | HF_SUB_IDX(idx) | VF_SUB_IDX(idx) | NF_MASK | previous < Z;
           '''),
    0xD9: ('EXX',
           '''
           u16_t tmp;
           tmp = BC; BC = BC_; BC_ = tmp;
           tmp = DE; DE = DE_; DE_ = tmp;
           tmp = HL; HL = HL_; HL_ = tmp;
           '''),
    0xDC: ('CALL C,nn', lambda: call('F & CF_MASK')),
    0xDD: {
        0x35: ('DEC (IX+d)',  lambda: dec_xy_d('IX')),
        0x4E: ('LD C,(IX+d)', lambda: ld_r_xy_d('C', 'IX')),
        0x66: ('LD H,(IX+d)', lambda: ld_r_xy_d('H', 'IX')),
        0x6E: ('LD L,(IX+d)', lambda: ld_r_xy_d('L', 'IX')),
        0x6F: ('LD IXL,A',    lambda: ld_r_r('IXL', 'A')),
        0x77: ('LD (IX+d),A', lambda: ld_xy_d_r('IX', 'A')),
        0x7D: ('LD A,IXL',    lambda: ld_r_r('A', 'IXL')),
        0x7E: ('LD A,(IX+d)', lambda: ld_r_xy_d('A', 'IX')),
        0xBE: ('CP (IX+d)',   lambda: cp_xy_d('IX')),
        0xCB: {
            # This is on the 4th byte, not the 3rd! PC remains at 3rd.
            0x46: ('BIT 0,(IX+d)', lambda: bit_b_xy_d(0, 'IX')),
            0x4E: ('BIT 1,(IX+d)', lambda: bit_b_xy_d(1, 'IX')),
            0x56: ('BIT 2,(IX+d)', lambda: bit_b_xy_d(2, 'IX')),
            0x5E: ('BIT 3,(IX+d)', lambda: bit_b_xy_d(3, 'IX')),
            0x66: ('BIT 4,(IX+d)', lambda: bit_b_xy_d(4, 'IX')),
            0x6E: ('BIT 5,(IX+d)', lambda: bit_b_xy_d(5, 'IX')),
            0x76: ('BIT 6,(IX+d)', lambda: bit_b_xy_d(6, 'IX')),
            0x7E: ('BIT 7,(IX+d)', lambda: bit_b_xy_d(7, 'IX')),
        },
    },
    0xE1: ('POP HL', lambda: pop_qq('HL')),
    0xE3: ('EX (SP),HL',
           '''
           Z = memory_read(SP);     T(3);
           W = memory_read(SP + 1); T(3);
           memory_write(SP,     L); T(4);
           memory_write(SP + 1, H); T(5);
           H = W;
           L = Z;
           '''),
    0xE5: ('PUSH HL', lambda: push_qq('HL')),
    0xE6: ('AND n',
           '''
           A &= memory_read(PC++); T(3);
           F = SZ53P(A) | (1 << HF_SHIFT);
           '''),
    0xE7: ('RST 20h', lambda: rst(0x20)),
    0xE9: ('JP (HL)',
           '''
           PC = HL;
           '''),
    0xEB: ('EX DE,HL', lambda: ex('DE', 'HL')),
    0xCB: {
        0x02: ('RLC D', lambda: rlc_r('D')),
        0x3F: ('SRL A', lambda: srl_r('A')),
    },
    0xED: {
        0x43: ('LD (nn),BC', lambda: ld_nn_dd('BC')),
        0x4B: ('LD BC,(nn)', lambda: ld_dd_nn('BC')),
        0x51: ('OUT (C),D',  lambda: out_c_r('D')),
        0x52: ('SBC HL,DE',  lambda: sbc_hl_ss('DE')),
        0x61: ('OUT (C),H',  lambda: out_c_r('H')),
        0x69: ('OUT (C),L',  lambda: out_c_r('L')),
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
        0x94: ('pixelad', None), # using D,E (as Y,X) calculate the ULA screen address and store in HL
        0xA2: ('INI', lambda: inx('+')),
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
    prefix         = prefix or []
    prefix_len     = len(prefix)
    prefix_str     = ''.join(f'{opcode:02X}h ' for opcode in prefix)
    prefix_comment = f'/* {prefix_str}*/ ' if prefix else ''

    if prefix == [0xDD, 0xCB] or prefix == [0xFD, 0xCB]:
        # Special opcode where 3rd byte is parameter and 4th byte needed for
        # decoding. Read 4th byte, but keep PC at 3rd byte.
        read_opcode = 'opcode = memory_read(PC + 1);'
    else:
        read_opcode = 'opcode = memory_read(PC++);'

    f.write(f'''
{read_opcode}
T(4);
switch (opcode) {{
''')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, spec = item
            c = spec() if callable(spec) else spec
            if c is not None:
                f.write(f'''
case {prefix_comment}0x{opcode:02X}:  /* {mnemonic} */
  /* fprintf(stderr, "%04Xh {mnemonic:20s} A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\\n", PC - 1 - {len(prefix)}, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c"); */
  {{
    {c}
  }}
  break;

''')

        else:
            f.write(f'case 0x{opcode:02X}:\n')
            generate(item, f, prefix + [opcode])

    optional_break = 'break;' if prefix else ''
    f.write(f'''
default:
  fprintf(stderr, "cpu: unknown opcode {prefix_str}%02Xh at %04Xh\\n", opcode, PC - 1 - {prefix_len});
  return -1;
}}
{optional_break}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
