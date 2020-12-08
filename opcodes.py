#!/usr/bin/env python3


import io
import re
from   typing import *


Opcode      = int
Mnemonic    = str
C           = Optional[str]
CGenerator  = Callable[[], C]
ReadAhead   = bool
Instruction = Tuple[Mnemonic, Union[C, CGenerator]]
Table       = Dict[Opcode, Union[Instruction, 'Table']]


def hi(dd: str) -> str:
    return f'{dd}H' if dd in ['IX', 'IY'] else dd[0]

def lo(dd: str) -> str:
    return f'{dd}L' if dd in ['IX', 'IY'] else dd[1]

def adc_a_r(r: str) -> C:
    return None
    
def add_a_r(r: str) -> C:
    return f'''
        const u16_t result = A + {r};
        const u8_t  carry  = A > 0xFF - {r};
        F = SZ53(A) | HF_ADD(A, {r}, result) | VF_ADD(A, {r}, result) | carry;
        A += {r} & 0xFF;
    '''

def add_dd_nn(dd: str) -> C:
    return f'''
        Z = memory_read(PC++); T(4);
        W = memory_read(PC++); T(4);
        {dd} += WZ;
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
        A &= {r};
        F = SZ53P(A) | (1 << HF_SHIFT);
    '''

def bit_b_r(b: int, r: str) -> C:
    return f'''
        F &= ~(ZF_MASK | NF_MASK);
        F |= ({r} & 1 << {b} ? 0 : ZF_MASK) | HF_MASK;
        T(4);
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
        const u16_t result = A - {r};
        F                  = SZ53(result & 0xFF) | HF_SUB(A, {r}, result) | VF_SUB(A, {r}, result) | NF_MASK | (A < {r}) << CF_SHIFT;
    '''

def cp_xy_d(xy: str) -> C:
    return f'''
        u16_t result;
        u8_t  tmp;
        WZ     = {xy} + (s8_t) memory_read(PC++); T(3);
        tmp    = memory_read(WZ);                 T(5);
        result = A - tmp;
        F      = SZ53(result & 0xFF) | HF_SUB({xy}, tmp, result) | VF_SUB({xy}, tmp, result) | NF_MASK | (A < tmp) << CF_SHIFT;
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
       const u8_t carry = F & CF_MASK;
       {r}++;
       F = SZ53({r}) | HF_ADD({r} - 1, 1, {r}) | ({r} == 0x80) << VF_SHIFT | carry;
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
    return f'''
        {lo(dd)} = memory_read(PC++); T(3);
        {hi(dd)} = memory_read(PC++); T(3);
    '''

def ld_dd_pnn(dd: str) -> C:
    return f'''
      Z = memory_read(PC++);          T(3);
      W = memory_read(PC++);          T(3);
      {lo(dd)} = memory_read(WZ);     T(3);
      {hi(dd)} = memory_read(WZ + 1); T(3);
    '''

def ld_pdd_r(dd: str, r: str) -> C:
    return f'memory_write({dd}, {r}); T(3);'

def ld_pnn_dd(dd: str) -> C:
    return f'''
        Z = memory_read(PC++);          T(3);
        W = memory_read(PC++);          T(3);
        memory_write(WZ,     {lo(dd)}); T(3);
        memory_write(WZ + 1, {hi(dd)}); T(3);
    '''

def ld_r_pdd(r: str, dd: str) -> C:
    return f'{r} = memory_read({dd}); T(3);'

def ld_r_n(r: str) -> C:
    return f'{r} = memory_read(PC++); T(3);'

def ld_r_r(r1: str, r2: str) -> C:
    return f'{r1} = {r2};'

def ld_r_xy_d(r: str, xy: str) -> C:
    return f'''
        WZ = {xy} + (s8_t) memory_read(PC++); T(3 + 5);
        {r} = memory_read(WZ); T(3);
    '''

def ld_xy_d_n(xy: str) -> C:
    return f'''
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        memory_write(WZ, memory_read(PC++));  T(5 + 3);
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
    return f'''
        {lo(qq)} = memory_read(SP++); T(3);
        {hi(qq)} = memory_read(SP++); T(3);
    '''

def push_qq(qq: str) -> C:
    return f'''
        T(1);
        memory_write(--SP, {hi(qq)}); T(3);
        memory_write(--SP, {lo(qq)}); T(3);
    '''

def res_b_xy_d(b: int, xy: str) -> C:
    return f'''
        T(1);
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        PC++;
        Z = memory_read(WZ) & ~(1 << {b});    T(4);
        memory_write(WZ, Z);                  T(3);
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

def set_b_r(b: int, r: str) -> C:
    return f'{r} |= 1 << {b};'

def set_b_xy_d(b: int, xy: str) -> C:
    return f'''
        T(1);
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        PC++;
        Z = memory_read(WZ) | 1 << {b}; T(4);
        memory_write(WZ, Z);            T(3);
    '''

def srl_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} >>= 1;
        F = SZ53P({r}) | carry;
    '''

def sub_r(r: str) -> C:
    return f'''
        const u16_t result = A - {r};
        F = SZ53(result & 0xFF) | HF_SUB(A, {r}, result) | VF_SUB(A, {r}, result) | NF_MASK | (A < {r});
        A = result & 0xFF;
    '''

def xor_r(r: str) -> C:
    return f'''
       A ^= {r};
       F = SZ53P(A);
    '''


def xy_table(xy: str) -> Table:
    return {
        0x21: (f'LD {xy},nn',    lambda: ld_dd_nn(xy)),
        0x23: (f'INC {xy}',      lambda: inc_ss(xy)),
        0x35: (f'DEC ({xy}+d)',  lambda: dec_xy_d(xy)),
        0x36: (f'LD ({xy}+d),n', lambda: ld_xy_d_n(xy)),
        0x4E: (f'LD C,({xy}+d)', lambda: ld_r_xy_d('C', xy)),
        0xE5: (f'PUSH {xy}',     lambda: push_qq(xy)),
        0x66: (f'LD H,({xy}+d)', lambda: ld_r_xy_d('H', xy)),
        0x6E: (f'LD L,({xy}+d)', lambda: ld_r_xy_d('L', xy)),
        0x6F: (f'LD {lo(xy)},A', lambda: ld_r_r(lo(xy), 'A')),
        0x77: (f'LD ({xy}+d),A', lambda: ld_xy_d_r(xy, 'A')),
        0x7D: (f'LD A,{lo(xy)}', lambda: ld_r_r('A', lo(xy))),
        0x7E: (f'LD A,({xy}+d)', lambda: ld_r_xy_d('A', xy)),
        0xBE: (f'CP ({xy}+d)',   lambda: cp_xy_d(xy)),
        0xCB: {
            # This is on the 4th byte, not the 3rd! PC remains at 3rd.
            0x46: (f'BIT 0,({xy}+d)', lambda: bit_b_xy_d(0, xy)),
            0x4E: (f'BIT 1,({xy}+d)', lambda: bit_b_xy_d(1, xy)),
            0x56: (f'BIT 2,({xy}+d)', lambda: bit_b_xy_d(2, xy)),
            0x5E: (f'BIT 3,({xy}+d)', lambda: bit_b_xy_d(3, xy)),
            0x66: (f'BIT 4,({xy}+d)', lambda: bit_b_xy_d(4, xy)),
            0x6E: (f'BIT 5,({xy}+d)', lambda: bit_b_xy_d(5, xy)),
            0x76: (f'BIT 6,({xy}+d)', lambda: bit_b_xy_d(6, xy)),
            0x7E: (f'BIT 7,({xy}+d)', lambda: bit_b_xy_d(7, xy)),
            0x86: (f'RES 0,({xy}+d)', lambda: res_b_xy_d(0, xy)),
            0x8E: (f'RES 1,({xy}+d)', lambda: res_b_xy_d(1, xy)),
            0x96: (f'RES 2,({xy}+d)', lambda: res_b_xy_d(2, xy)),
            0x9E: (f'RES 3,({xy}+d)', lambda: res_b_xy_d(3, xy)),
            0xA6: (f'RES 4,({xy}+d)', lambda: res_b_xy_d(4, xy)),
            0xAE: (f'RES 5,({xy}+d)', lambda: res_b_xy_d(5, xy)),
            0xB6: (f'RES 6,({xy}+d)', lambda: res_b_xy_d(6, xy)),
            0xBE: (f'RES 7,({xy}+d)', lambda: res_b_xy_d(7, xy)),
            0xC6: (f'SET 0,({xy}+d)', lambda: set_b_xy_d(0, xy)),
            0xCE: (f'SET 1,({xy}+d)', lambda: set_b_xy_d(1, xy)),
            0xD6: (f'SET 2,({xy}+d)', lambda: set_b_xy_d(2, xy)),
            0xDE: (f'SET 3,({xy}+d)', lambda: set_b_xy_d(3, xy)),
            0xE6: (f'SET 4,({xy}+d)', lambda: set_b_xy_d(4, xy)),
            0xEE: (f'SET 5,({xy}+d)', lambda: set_b_xy_d(5, xy)),
            0xF6: (f'SET 6,({xy}+d)', lambda: set_b_xy_d(6, xy)),
            0xFE: (f'SET 7,({xy}+d)', lambda: set_b_xy_d(7, xy)),
        },
        0xE1: (f'POP {xy}', lambda: pop_qq(xy)),
    }

# See https://wiki.specnext.dev/Extended_Z80_instruction_set.
instructions: Table = {
    0x00: ('NOP',       ''),
    0x01: ('LD BC,nn',  lambda: ld_dd_nn('BC')),
    0x03: ('INC BC',    lambda: inc_ss('BC')),
    0x04: ('INC B',     lambda: inc_r('B')),
    0x05: ('DEC B',     lambda: dec_r('B')),
    0x06: ('LD B,n',    lambda: ld_r_n('B')),
    0x07: ('RLCA',
           '''
           const u8_t carry = A >> 7;;
           F &= ~(NF_MASK | CF_MASK);
           F |= HF_MASK | carry << CF_SHIFT;
           A <<= 1;
           A |= carry;
           '''),
    0x08: ("EX AF,AF'", lambda: ex('AF', 'AF_')),
    0x0A: ('LD A,(BC)', lambda: ld_r_pdd('A', 'BC')),
    0x0B: ('DEC BC',    lambda: dec_ss('BC')),
    0x0C: ('INC C',     lambda: inc_r('C')),
    0x0D: ('DEC C',     lambda: dec_r('C')),
    0x0E: ('LD C,n',    lambda: ld_r_n('C')),
    0x10: ('DJNZ e',
           f'''
           T(1);
           Z = memory_read(PC++); T(3);
           if (--B) {{
             PC += (s8_t) Z; T(5);
           }}
           '''),
    0x11: ('LD DE,nn',  lambda: ld_dd_nn('DE')),
    0x12: ('LD (DE),A', lambda: ld_pdd_r('DE', 'A')),
    0x13: ('INC DE',    lambda: inc_ss('DE')),
    0x14: ('INC D',     lambda: inc_r('D')),
    0x15: ('DEC D',     lambda: dec_r('D')),
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
    0x19: ('ADD HL,DE',  lambda: add_hl_ss('DE')),
    0x1A: ('LD A,(DE)',  lambda: ld_r_pdd('A', 'DE')),
    0x1B: ('DEC DE',     lambda: dec_ss('DE')),
    0x1D: ('DEC E',      lambda: dec_r('E')),
    0x1E: ('LD E,n',     lambda: ld_r_n('E')),
    0x20: ('JR NZ,e',    lambda: jr_c_e('!ZF')),
    0x21: ('LD HL,nn',   lambda: ld_dd_nn('HL')),
    0x22: ('LD (nn),HL', lambda: ld_pnn_dd('HL')),
    0x23: ('INC HL',     lambda: inc_ss('HL')),
    0x25: ('DEC H',      lambda: dec_r('H')),
    0x26: ('LD H,n',     lambda: ld_r_n('H')),
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
    0x2C: ('INC L',      lambda: inc_r('L')),
    0x2E: ('LD L,n',     lambda: ld_r_n('L')),
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
    0x37: ('SCF',
           '''
           F &= ~(HF_MASK | NF_MASK);
           F |= CF_MASK;
           '''),
    0x38: ('JR C,e',    lambda: jr_c_e('CF')),
    0x39: ('ADD HL,SP', lambda: add_hl_ss('SP')),
    0x3A: ('LD A,(nn)',
           '''
           Z = memory_read(PC++); T(3);
           W = memory_read(PC++); T(3);
           A = memory_read(WZ);   T(3);
           '''),
    0x3C: ('INC A',     lambda: inc_r('A')),
    0x3D: ('DEC A',     lambda: dec_r('A')),
    0x3E: ('LD A,n',    lambda: ld_r_n('A')),
    0x44: ('LD B,H',    lambda: ld_r_r('B', 'H')),
    0x46: ('LD B,(HL)', lambda: ld_r_pdd('B', 'HL')),
    0x47: ('LD B,A',    lambda: ld_r_r('B', 'A')),
    0x4B: ('LD C,E',    lambda: ld_r_r('C', 'B')),
    0x4D: ('LD C,L',    lambda: ld_r_r('C', 'L')),
    0x4E: ('LD C,(HL)', lambda: ld_r_pdd('C', 'HL')),
    0x4F: ('LD C,A',    lambda: ld_r_r('C', 'A')),
    0x52: ('LD D,D',    lambda: ld_r_r('D', 'D')),
    0x54: ('LD D,H',    lambda: ld_r_r('D', 'H')),
    0x56: ('LD D,(HL)', lambda: ld_r_pdd('D', 'HL')),
    0x57: ('LD D,A',    lambda: ld_r_r('D', 'A')),
    0x58: ('LD E,B',    lambda: ld_r_r('E', 'B')),
    0x5E: ('LD E,(HL)', lambda: ld_r_pdd('E', 'HL')),
    0x5F: ('LD E,A',    lambda: ld_r_r('E', 'A')),
    0x62: ('LD H,D',    lambda: ld_r_r('H', 'D')),
    0x67: ('LD H,A',    lambda: ld_r_r('H', 'A')),
    0x6B: ('LD L,E',    lambda: ld_r_r('L', 'E')),
    0x6F: ('LD L,A',    lambda: ld_r_r('L', 'A')),
    0x70: ('LD (HL),B', lambda: ld_pdd_r('HL', 'B')),
    0x71: ('LD (HL),C', lambda: ld_pdd_r('HL', 'C')),
    0x75: ('LD (HL),L', lambda: ld_pdd_r('HL', 'L')),
    0x77: ('LD (HL),A', lambda: ld_pdd_r('HL', 'A')),
    0x78: ('LD A,B',    lambda: ld_r_r('A', 'B')),
    0x79: ('LD A,C',    lambda: ld_r_r('A', 'C')),
    0x7A: ('LD A,D',    lambda: ld_r_r('A', 'D')),
    0x7C: ('LD A,H',    lambda: ld_r_r('A', 'H')),
    0x7D: ('LD A,L',    lambda: ld_r_r('A', 'L')),
    0x7E: ('LD A,(HL)', lambda: ld_r_pdd('A', 'HL')),
    0x80: ('ADD A,B',   lambda: add_a_r('B')),
    0x87: ('ADD A,A',   lambda: add_a_r('A')),
    0x93: ('SUB E',     lambda: sub_r('E')),
    0xA2: ('AND D',     lambda: and_r('D')),
    0xA3: ('AND E',     lambda: and_r('E')),
    0xA7: ('AND A',     lambda: and_r('A')),
    0xA9: ('XOR C',     lambda: xor_r('C')),
    0xAF: ('XOR A',     lambda: xor_r('A')),
    0xB2: ('OR D',      lambda: or_r('D')),
    0xB3: ('OR E',      lambda: or_r('E')),
    0xB9: ('CP C',      lambda: cp_r('C')),
    0xBC: ('CP H',      lambda: cp_r('H')),
    0xC1: ('POP BC',    lambda: pop_qq('BC')),
    0xC2: ('JP NZ,nn',  lambda: jp('!(F & ZF_MASK)')),
    0xC3: ('JP nn',     jp),
    0xC4: ('CALL NZ,nn', lambda: call('!(F & ZF_MASK)')),
    0xC5: ('PUSH BC',    lambda: push_qq('BC')),
    0xC6: ('ADD A,n',
           '''
           const u8_t n       = memory_read(PC++); T(3);
           const u16_t result = A + n;
           const u8_t  carry  = A > 255 - n;
           F = SZ53(result & 0xFF) | HF_ADD(A, n, result) | VF_ADD(A, n, result) | carry;
           A = result & 0xFF;
           '''),
    0xC8: ('RET Z',      lambda: ret('F & ZF_MASK')),
    0xC9: ('RET',        ret),
    0xCB: {
        0x02: ('RLC D',   lambda: rlc_r('D')),
        0x3F: ('SRL A',   lambda: srl_r('A')),
        0x59: ('BIT 3,C', lambda: bit_b_r(3, 'C')),
        0x72: ('BIT 6,D', lambda: bit_b_r(6, 'D')),
        0xF2: ('SET 6,D', lambda: set_b_r(6, 'D')),
    },
    0xCD: ('CALL nn', call),
    0xD0: ('RET NC',  lambda: ret('!(F & CF_MASK)')),
    0xD1: ('POP DE',  lambda: pop_qq('DE')),
    0xD3: ('OUT (n),A',
           '''
           WZ = A << 8 | memory_read(PC++); T(3);
           io_write(WZ, A);                 T(4);
           '''),
    0xD4: ('CALL NC,nn', lambda: call('!(F & CF_MASK)')),
    0xD5: ('PUSH DE',    lambda: push_qq('DE')),
    0xD6: ('SUB n',
           '''
           u16_t result;
           Z      = memory_read(PC++); T(3);
           result = A - Z;
           F     = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | VF_SUB(A, Z, result) | NF_MASK | A < Z;
           A     = result & 0xFF;
           '''),
    0xD8: ('RET C', lambda: ret('F & CF_MASK')),
    0xD9: ('EXX',
           '''
           u16_t tmp;
           tmp = BC; BC = BC_; BC_ = tmp;
           tmp = DE; DE = DE_; DE_ = tmp;
           tmp = HL; HL = HL_; HL_ = tmp;
           '''),
    0xDC: ('CALL C,nn', lambda: call('F & CF_MASK')),
    0xDD: xy_table('IX'),
    0xDF: ('RST $18', lambda: rst(0x18)),
    0xE1: ('POP HL',  lambda: pop_qq('HL')),
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
    0xE7: ('RST $20', lambda: rst(0x20)),
    0xE9: ('JP (HL)',
           '''
           PC = HL;
           '''),
    0xEB: ('EX DE,HL', lambda: ex('DE', 'HL')),
    0xEF: ('RST $28',  lambda: rst(0x28)),
    0xED: {
        0x30: ('MUL D,E', 'DE = D * E;'),
        0x35: ('ADD DE,nn',  lambda: add_dd_nn('DE')),
        0x43: ('LD (nn),BC', lambda: ld_pnn_dd('BC')),
        0x4B: ('LD BC,(nn)', lambda: ld_dd_pnn('BC')),
        0x51: ('OUT (C),D',  lambda: out_c_r('D')),
        0x52: ('SBC HL,DE',  lambda: sbc_hl_ss('DE')),
        0x56: ('IM 1', 'IM = 1;'),
        0x59: ('OUT (C),E',  lambda: out_c_r('E')),
        0x61: ('OUT (C),H',  lambda: out_c_r('H')),
        0x69: ('OUT (C),L',  lambda: out_c_r('L')),
        0x73: ('LD (nn),SP', lambda: ld_pnn_dd('SP')),
        0x78: ('IN A,(C)',
               f'''
               A = io_read(BC); T(4);
               F = SZ53P(A);
               '''),
        0x79: ('OUT (C),A', lambda: out_c_r('A')),
        0x8A: ('PUSH mm',
               '''
               W = memory_read(PC++);  /* High byte first. */
               Z = memory_read(PC++);  /* Then low byte.   */
               memory_write(--SP, W);
               memory_write(--SP, Z);
               T(15);
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
        0x94: ('PIXELAD', None), # using D,E (as Y,X) calculate the ULA screen address and store in HL
        0xA2: ('INI',  lambda: inx('+')),
        0xB0: ('LDIR', lambda: ldxr('+')),
        0xB8: ('LDDR', lambda: ldxr('-')),
    },
    0xEE: ('XOR n',
           '''
           A ^= memory_read(PC++); T(3);
           F = SZ53P(A);
           '''),
    0xF1: ('POP AF',  lambda: pop_qq('AF')),
    0xF3: ('DI', 'IFF1 = IFF2 = 0;'),
    0xF5: ('PUSH AF', lambda: push_qq('AF')),
    0xF6: ('OR n',
           '''
           A |= memory_read(PC++); T(3);
           F = SZ53P(A);
           '''),
    0xF9: ('LD SP,HL',
           '''
           SP = HL; T(2);
           '''),
    0xFB: ('EI', 'IFF1 = IFF2 = 1;'),  # TODO: enabled maskable interrupt only AFTER NEXT instruction
    0xFD: xy_table('IY'),
    0xFE: ('CP n',
           '''
           u16_t result;
           Z      = memory_read(PC++);
           result = A - Z;
           F      = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | VF_SUB(A, Z, result) | NF_MASK | A < Z;
           T(3);
           ''')
}


def make_disassembler(mnemonic: str) -> str:
    tokens = {
        'n'    : (2, lambda offset: f'memory_read(PC + {offset})'),
        'nn'   : (4, lambda offset: f'memory_read(PC + {offset} + 1) << 8 | memory_read(PC + {offset})'),
        'mm'   : (4, lambda offset: f'memory_read(PC + {offset}) << 8 | memory_read(PC + {offset} + 1)'),
        'e'    : (4, lambda offset: f'PC + {offset} + 1 + (s8_t) memory_read(PC + {offset})'),
        'd'    : (2, lambda offset: f'memory_read(PC + {offset})'),
        'reg'  : (2, lambda offset: f'memory_read(PC + {offset})'),
        'value': (2, lambda offset: f'memory_read(PC + {offset} + 1)'),
    }

    statement = 'fprintf(stderr, "'
    args      = []

    offset = 0
    for part in re.split('([a-z]+)', mnemonic):
        if part in tokens:
            width, argument = tokens[part]
            statement += f'$%0{width}X'
            args.append(argument(offset))
            offset += width // 2
        else:
            statement += part

    statement += '\\n"'
    if args:
        statement += ', '
        statement += ', '.join(args)
    statement += ');'

    return statement
    
    
def generate(instructions: Table, f: io.TextIOBase, prefix: Optional[List[Opcode]] = None) -> None:
    prefix         = prefix or []
    prefix_len     = len(prefix)
    prefix_str     = ''.join(f'${opcode:02X} ' for opcode in prefix)
    prefix_comment = f'/* {prefix_str}*/ ' if prefix else ''

    if not prefix:
        f.write('''
fprintf(stderr, "     AF %04X BC %04X DE %04X HL %04X IX %04X IY %04X F %s%s-%s-%s%s%s\\n", AF, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
fprintf(stderr, "     AF'%04X BC'%04X DE'%04X HL'%04X PC %04X SP %04X I %02X\\n", AF_, BC_, DE_, HL_, PC, SP, I);
fprintf(stderr, "%04X ", PC);
''')

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
            disassembler   = make_disassembler(mnemonic)
            c = spec() if callable(spec) else spec
            if c is not None:
                f.write(f'''
case {prefix_comment}0x{opcode:02X}:  /* {mnemonic} */
  {{
    {disassembler}
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
  fprintf(stderr, "cpu: unknown opcode {prefix_str}$%02X at $%04X\\n", opcode, PC - 1 - {prefix_len});
  return -1;
}}
{optional_break}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(instructions, f)


if __name__ == '__main__':
    main()
