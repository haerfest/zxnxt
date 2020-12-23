#!/usr/bin/env python3


import io
import re
from functools import partial
from itertools import count
from typing    import *


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
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u16_t result = A + {r} + carry;
        F = SZ53(A) | HF_ADD(A, {r} + carry, result) | VF_ADD(A, {r} + carry, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A = result & 0xFF;
    '''

def adc_a_phl() -> C:
    return '''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u8_t  tmp    = memory_read(HL);
        const u16_t result = A + tmp + carry;
        F = SZ53(result & 0xFF) | HF_ADD(A, tmp + carry, result) | VF_ADD(A, tmp + carry, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A = result & 0xFF;
        T(3);
    '''

    return None

def adc_a_xy_d(xy: str) -> C:
    return f'''
        const u8_t carry  = (F & CF_MASK) >> CF_SHIFT;
        u16_t      result;
        u8_t       tmp;
        WZ     = {xy} + (s8_t) memory_read(PC++); T(3);
        tmp    = memory_read(WZ);                 T(5);
        result = A + tmp + carry;
        F      = SZ53(result & 0xFF) | HF_ADD(A, tmp + carry, result) | VF_ADD(A, tmp + carry, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A      = result & 0xFF;
        T(3);
    '''

def adc_hl_ss(ss: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u32_t result = HL + {ss} + carry; T(4);
        F  = (result & 0x8000) >> 15 << SF_SHIFT | (result == 0) << ZF_SHIFT | (result & 0x20) | (result & 0x08) | HF_ADD(H, {hi(ss)}, result >> 8) | VF_ADD(H, {hi(ss)}, result >> 8) | (result & 0x10000) >> 16 << CF_SHIFT;
        HL = result & 0xFFFF; T(3);
    '''

def add_a_r(r: str) -> C:
    return f'''
        const u16_t result = A + {r};
        const u8_t  carry  = A > 0xFF - {r};
        F = SZ53(A) | HF_ADD(A, {r}, result) | VF_ADD(A, {r}, result) | carry;
        A += {r} & 0xFF;
    '''

def add_a_phl() -> C:
    return '''
        const u8_t  tmp    = memory_read(HL);
        const u16_t result = A + tmp;
        F      = SZ53(result & 0xFF) | HF_ADD(A, tmp, result) | VF_ADD(A, tmp, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A      = result & 0xFF;
        T(3);
    '''

def add_a_xy_d(xy: str) -> C:
    return f'''
        u16_t result;
        u8_t  tmp;
        WZ     = {xy} + (s8_t) memory_read(PC++); T(3);
        tmp    = memory_read(WZ);                 T(5);
        result = A + tmp;
        F      = SZ53(result & 0xFF) | HF_ADD(A, tmp, result) | VF_ADD(A, tmp, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A      = result & 0xFF;
        T(3);
    '''

def add_dd_nn(dd: str) -> C:
    return f'''
        Z = memory_read(PC++); T(4);
        W = memory_read(PC++); T(4);
        {dd} += WZ;
    '''

def add_dd_r(dd: str, r: str) -> C:
    return f'{dd} += {r};'

def add_hl_ss(ss: str) -> C:
    return f'''
        const u16_t prev  = HL;
        const u8_t  carry = HL > 0xFFFF - {ss};
        HL += {ss}; T(4 + 3);
        F &= ~(HF_MASK | NF_MASK | CF_MASK);
        F |= HF_ADD(prev >> 8, {ss} >> 8, HL >> 8) | carry;
    '''

def add_xy_rr(xy: str, rr: str) -> C:
    return f'''
        const u32_t result = {xy} + {rr};
        F &= NF_MASK;
        F |= HF_ADD({xy} >> 8, {rr} >> 8, result >> 8) | (result >> 16) << CF_SHIFT;
        {xy} = result & 0xFFFF;
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

def bit_b_phl(b: int) -> C:
    return f'''
        F &= ~(ZF_MASK | NF_MASK);
        F |= (memory_read(HL) & 1 << {b} ? 0 : ZF_MASK) | HF_MASK; T(4);
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
        F      = SZ53(result & 0xFF) | HF_SUB(A, tmp, result) | VF_SUB(A, tmp, result) | NF_MASK | (A < tmp) << CF_SHIFT;
    '''

def dec_r(r: str) -> C:
    return f'''
        {r}--;
        F = SZ53({r}) | HF_SUB({r} + 1, 1, {r}) | ({r} == 0x79) << VF_SHIFT | NF_MASK | (F & CF_MASK);
    '''

def dec_ss(ss: str) -> C:
    return f'{ss}--; T(2);'

def dec_xy_d(xy: str) -> C:
    return f'''
        u8_t m;
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        m = memory_read(WZ) - 1;              T(5);
        memory_write(WZ, m);                  T(4);
        F = SZ53(m) | HF_SUB(m + 1, 1, m) | (m == 0x79) << VF_SHIFT | NF_MASK | (F & CF_MASK);
        T(3);
    '''

def ex(r1: str, r2: str) -> C:
    return f'''
        const u16_t tmp = {r1};
        {r1} = {r2};
        {r2} = tmp;
    '''

def ex_psp_dd(dd: str) -> C:
    return f'''
        Z = memory_read(SP);            T(3);
        W = memory_read(SP + 1);        T(3);
        memory_write(SP,     {lo(dd)}); T(4);
        memory_write(SP + 1, {hi(dd)}); T(5);
        {hi(dd)} = W;
        {lo(dd)} = Z;
    '''

def in_r_pc(r: str) -> C:
    return f'''
        {r} = io_read(BC); T(4);
        F = SZ53P({r});
    '''

def inc_phl() -> C:
    return '''
      Z = memory_read(HL) + 1; T(4);
      memory_write(HL, Z);     T(3);
      F = SZ53(Z) | HF_ADD(Z - 1, 1, Z) | (Z == 0x80) << VF_SHIFT | (F & CF_MASK);
    '''

def inc_r(r: str) -> C:
    return f'''
       {r}++;
       F = SZ53({r}) | HF_ADD({r} - 1, 1, {r}) | ({r} == 0x80) << VF_SHIFT | (F & CF_MASK);
    '''

def inc_xy_d(xy: str) -> C:
    return f'''
        u8_t m;
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        m = memory_read(WZ) + 1;              T(5);
        memory_write(WZ, m);                  T(4);
        F = SZ53(m) | HF_ADD(m - 1, 1, m) | (m == 0x80) << VF_SHIFT | (F & CF_MASK);
        T(3);
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

def or_xy_d(xy: str) -> C:
    return f'''
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        A |= memory_read(WZ);                 T(5);
        F = SZ53P(A);                         T(3);
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

def res_b_phl(b: int) -> C:
    return f'''
        memory_write(HL, memory_read(HL) & ~(1 << {b})); T(4 + 3);
    '''

def res_b_r(b: int, r: str) -> C:
    return f'{r} &= ~(1 << {b});'

def res_b_xy_d(b: int, xy: str) -> C:
    return f'''
        T(1);
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        PC++;
        memory_write(WZ, memory_read(WZ) & ~(1 << {b})); T(4 + 3);
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

def rl_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z >> 7;
        Z = Z << 1 | (F & CF_MASK) >> CF_SHIFT;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def rl_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} = {r} << 1 | (F & CF_MASK) >> CF_SHIFT;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rlc_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z >> 7;
        Z = Z << 1 | carry;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def rlc_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} = {r} << 1 | carry;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rr_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z & 0x01;
        Z = Z >> 1 | (F & CF_MASK) >> CF_SHIFT << 7;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def rr_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = {r} >> 1 | (F & CF_MASK) >> CF_SHIFT << 7;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rrc_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z & 0x01;
        Z = Z >> 1 | carry << 7;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def rrc_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = {r} >> 1 | carry << 7;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rst(address: int) -> C:
    return f'''
        T(1);
        memory_write(--SP, PCH); T(3);
        memory_write(--SP, PCL);
        PC = 0x{address:02X};    T(3);
    '''

def sbc_hl_ss(ss: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u32_t result = HL - {ss} - carry; T(4);
        F  = (result & 0x8000) >> 15 << SF_SHIFT | (result == 0) << ZF_SHIFT | (result & 0x20) | (result & 0x08) | HF_SUB(H, {hi(ss)}, result >> 8) | VF_SUB(H, {hi(ss)}, result >> 8) | NF_MASK | (HL < {ss} + carry) << CF_SHIFT;
        HL = result & 0xFFFF; T(3);
    '''

def sbc_r(r: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u16_t result = A - {r} - carry;
        F = SZ53(A) | HF_SUB(A, {r} + carry, result) | VF_SUB(A, {r} + carry, result) | NF_MASK | (A < {r} + carry) << CF_SHIFT;
        A = result & 0xFF;
    '''
    
def set_b_phl(b: int) -> C:
    return f'''
      memory_write(HL, memory_read(HL) | 1 << {b}); T(4 + 3);
    '''

def set_b_r(b: int, r: str) -> C:
    return f'{r} |= 1 << {b};'

def set_b_xy_d(b: int, xy: str) -> C:
    return f'''
        T(1);
        WZ = {xy} + (s8_t) memory_read(PC++); T(3);
        PC++;
        memory_write(WZ, memory_read(WZ) | 1 << {b}); T(4 + 3);
    '''

def sla_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z >> 7;
        Z <<= 1;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def sla_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} <<= 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def sra_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z & 0x01;
        Z = Z & 0x80 | Z >> 1;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def sra_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = {r} & 0x80 | {r} >> 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def srl_phl() -> C:
    return '''
        u8_t carry;
        Z = memory_read(HL);
        carry = Z & 0x01;
        Z >>= 1;
        F = SZ53P(Z) | carry << CF_SHIFT;
        T(4);
        memory_write(HL, Z);
        T(3);
    '''

def srl_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} >>= 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def sub_r(r: str) -> C:
    return f'''
        const u16_t result = A - {r};
        F = SZ53(result & 0xFF) | HF_SUB(A, {r}, result) | VF_SUB(A, {r}, result) | NF_MASK | (A < {r}) << CF_SHIFT;
        A = result & 0xFF;
    '''

def sub_xy_d(xy: str) -> C:
    return f'''
        u16_t result;
        u8_t  tmp;
        WZ     = {xy} + (s8_t) memory_read(PC++); T(3);
        tmp    = memory_read(WZ);                 T(5);
        result = A - tmp;
        F      = SZ53(result & 0xFF) | HF_SUB(A, tmp, result) | VF_SUB(A, tmp, result) | NF_MASK | (A < tmp) << CF_SHIFT;
        A      = result & 0xFF;
        T(3);
    '''

def xor_r(r: str) -> C:
    return f'''
       A ^= {r};
       F = SZ53P(A);
    '''


def cb_table() -> Table:
    table = {}

    def _bit_twiddling(mnemonic: str, B_opcode: int, act_b_r: Callable[[int, str], C], act_b_phl: Callable[[int, str], C]) -> None:
        for top_opcode, r in zip(range(B_opcode, B_opcode + 6), 'BCDEHL'):
            for opcode, b in zip(count(top_opcode, 8), range(8)):
                table[opcode] = (f'{mnemonic} {b},{r}', partial(act_b_r, b, r))

        for opcode, b in zip(count(B_opcode + 6, 8), range(8)):
            table[opcode] = (f'{mnemonic} {b},(HL)', partial(act_b_phl, b))

        for opcode, b in zip(count(B_opcode + 7, 8), range(8)):
            table[opcode] = (f'{mnemonic} {b},A', partial(act_b_r, b, 'A'))

    def _rotates(B_opcode: int) -> None:
        actions = [
            ('RLC', rlc_r, rlc_phl),
            ('RRC', rrc_r, rrc_phl),
            ('RL',  rl_r,  rl_phl),
            ('RR',  rr_r,  rr_phl),
            ('SLA', sla_r, sla_phl),
            ('SRA', sra_r, sra_phl),
            ('SRL', srl_r, srl_phl)
        ]

        for top_opcode, r in zip(range(B_opcode, B_opcode + 6), 'BCDEHL'):
            deltas = [0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x38]
            for delta, (mnemonic, act_r, act_phl) in zip(deltas, actions):
                table[top_opcode + delta] = (f'{mnemonic} {r}', partial(act_r, r))

        for delta, (mnemonic, act_r, act_phl) in zip(deltas, actions):
            table[B_opcode + 6 + delta] = (f'{mnemonic} (HL)', act_phl)

        for delta, (mnemonic, act_r, act_phl) in zip(deltas, actions):
            table[B_opcode + 7 + delta] = (f'{mnemonic} A', partial(act_r, 'A'))

    _bit_twiddling('BIT', 0x40, bit_b_r, bit_b_phl)
    _bit_twiddling('RES', 0x80, res_b_r, res_b_phl)
    _bit_twiddling('SET', 0xC0, set_b_r, set_b_phl)

    _rotates(0x00)

    return table


def ed_table() -> Table:
    return {
        0x30: ('MUL D,E', 'DE = D * E;'),
        0x31: ('ADD HL,A',   partial(add_dd_r, 'HL', 'A')),
        0x34: ('ADD HL,nn',  partial(add_dd_nn, 'HL')),
        0x35: ('ADD DE,nn',  partial(add_dd_nn, 'DE')),
        0x41: ('OUT (C),B',  partial(out_c_r, 'B')),
        0x42: ('SBC HL,BC',  partial(sbc_hl_ss, 'BC')),
        0x43: ('LD (nn),BC', partial(ld_pnn_dd, 'BC')),
        0x44: ('NEG',
               '''
               const u8_t prev = A;
               A = -A;
               F = SZ53(A) | HF_SUB(0, prev, A) | (prev == 0x80) << VF_SHIFT | NF_MASK | (prev != 0x00) << CF_SHIFT;
               '''),
        0x47: ('LD I,A',
               '''
               T(1);
               I = A;
               '''),
        0x4A: ('ADC HL,BC',  partial(adc_hl_ss, 'BC')),
        0x4B: ('LD BC,(nn)', partial(ld_dd_pnn, 'BC')),
        0x51: ('OUT (C),D',  partial(out_c_r, 'D')),
        0x52: ('SBC HL,DE',  partial(sbc_hl_ss, 'DE')),
        0x53: ('LD (nn),DE', partial(ld_pnn_dd, 'DE')),
        0x56: ('IM 1', 'IM = 1;'),
        0x57: ('LD A,I',
               '''
               T(1);
               A = I;
               F = SZ53(A) | (IFF2 << PF_SHIFT) | (F & CF_MASK);
               '''),
        0x59: ('OUT (C),E',  partial(out_c_r, 'E')),
        0x5A: ('ADC HL,DE',  partial(adc_hl_ss, 'DE')),
        0x5B: ('LD DE,(nn)', partial(ld_dd_pnn, 'DE')),
        0x5F: ('LD A,R',
               '''
               T(1);
               A = R;
               F = SZ53(A) | (IFF2 << VF_SHIFT) | (F & CF_MASK) >> CF_SHIFT;
               '''),
        0x61: ('OUT (C),H',  partial(out_c_r, 'H')),
        0x68: ('IN L,(C)',   partial(in_r_pc, 'L')),
        0x69: ('OUT (C),L',  partial(out_c_r, 'L')),
        0x6A: ('ADC HL,HL',  partial(adc_hl_ss, 'HL')),
        0x73: ('LD (nn),SP', partial(ld_pnn_dd, 'SP')),
        0x78: ('IN A,(C)',   partial(in_r_pc, 'A')),
        0x79: ('OUT (C),A',  partial(out_c_r, 'A')),
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
        0xA2: ('INI',  partial(inx,  '+')),
        0xB0: ('LDIR', partial(ldxr, '+')),
        0xB2: ('INIR',
               '''
               T(1);
               Z = io_read(BC);     T(3);
               memory_write(HL, Z); T(4);
               HL++;
               B--;
               if (B == 0) {
                 F |= ZF_MASK | NF_MASK;
               } else {
                 PC -= 2; T(5);
                 R++;
               }
               '''),
        0xB8: ('LDDR', partial(ldxr, '-')),
    }


def xy_table(xy: str) -> Table:
    return {
        0x09: (f'ADD {xy},BC',    partial(add_xy_rr, xy, 'BC')),
        0x19: (f'ADD {xy},DE',    partial(add_xy_rr, xy, 'DE')),
        0x21: (f'LD {xy},nn',     partial(ld_dd_nn, xy)),
        0x22: (f'LD (nn),{xy}',   partial(ld_pnn_dd, xy)),
        0x23: (f'INC {xy}',       partial(inc_ss, xy)),
        0x26: (f'LD {hi(xy)},n',  partial(ld_r_n, hi(xy))),
        0x2A: (f'LD {xy},(nn)',   partial(ld_dd_pnn, xy)),
        0x34: (f'INC ({xy}+d)',   partial(inc_xy_d, xy)),
        0x35: (f'DEC ({xy}+d)',   partial(dec_xy_d, xy)),
        0x36: (f'LD ({xy}+d),n',  partial(ld_xy_d_n, xy)),
        0x46: (f'LD B,({xy}+d)',  partial(ld_r_xy_d, 'B', xy)),
        0x4E: (f'LD C,({xy}+d)',  partial(ld_r_xy_d, 'C', xy)),
        0x54: (f'LD D,{hi(xy)}',  partial(ld_r_r, 'D', hi(xy))),
        0x56: (f'LD D,({xy}+d)',  partial(ld_r_xy_d, 'D', xy)),
        0x5D: (f'LD E,{lo(xy)}',  partial(ld_r_r, 'E', lo(xy))),
        0x5E: (f'LD E,({xy}+d)',  partial(ld_r_xy_d, 'E', xy)),
        0x66: (f'LD H,({xy}+d)',  partial(ld_r_xy_d, 'H', xy)),
        0x67: (f'LD {hi(xy)},A',  partial(ld_r_r, hi(xy), 'A')),
        0x6E: (f'LD L,({xy}+d)',  partial(ld_r_xy_d, 'L', xy)),
        0x6F: (f'LD {lo(xy)},A',  partial(ld_r_r, lo(xy), 'A')),
        0x70: (f'LD ({xy}+d),B',  partial(ld_xy_d_r, xy, 'B')),
        0x71: (f'LD ({xy}+d),C',  partial(ld_xy_d_r, xy, 'C')),
        0x72: (f'LD ({xy}+d),D',  partial(ld_xy_d_r, xy, 'D')),
        0x73: (f'LD ({xy}+d),E',  partial(ld_xy_d_r, xy, 'E')),
        0x74: (f'LD ({xy}+d),H',  partial(ld_xy_d_r, xy, 'H')),
        0x75: (f'LD ({xy}+d),L',  partial(ld_xy_d_r, xy, 'L')),
        0x77: (f'LD ({xy}+d),A',  partial(ld_xy_d_r, xy, 'A')),
        0x7C: (f'LD A,{hi(xy)}',  partial(ld_r_r, 'A', hi(xy))),
        0x7D: (f'LD A,{lo(xy)}',  partial(ld_r_r, 'A', lo(xy))),
        0x7E: (f'LD A,({xy}+d)',  partial(ld_r_xy_d, 'A', xy)),
        0x86: (f'ADD A,({xy}+d)', partial(add_a_xy_d, xy)),
        0x8E: (f'ADC A,({xy}+d)', partial(adc_a_xy_d, xy)),
        0x96: (f'SUB ({xy}+d)',   partial(sub_xy_d, xy)),
        0xB6: (f'OR ({xy}+d)',    partial(or_xy_d, xy)),
        0xBE: (f'CP ({xy}+d)',    partial(cp_xy_d, xy)),
        0xE5: (f'PUSH {xy}',      partial(push_qq, xy)),
        0xCB: {
            # This is on the 4th byte, not the 3rd! PC remains at 3rd.
            0x46: (f'BIT 0,({xy}+d)', partial(bit_b_xy_d, 0, xy)),
            0x4E: (f'BIT 1,({xy}+d)', partial(bit_b_xy_d, 1, xy)),
            0x56: (f'BIT 2,({xy}+d)', partial(bit_b_xy_d, 2, xy)),
            0x5E: (f'BIT 3,({xy}+d)', partial(bit_b_xy_d, 3, xy)),
            0x66: (f'BIT 4,({xy}+d)', partial(bit_b_xy_d, 4, xy)),
            0x6E: (f'BIT 5,({xy}+d)', partial(bit_b_xy_d, 5, xy)),
            0x76: (f'BIT 6,({xy}+d)', partial(bit_b_xy_d, 6, xy)),
            0x7E: (f'BIT 7,({xy}+d)', partial(bit_b_xy_d, 7, xy)),
            0x86: (f'RES 0,({xy}+d)', partial(res_b_xy_d, 0, xy)),
            0x8E: (f'RES 1,({xy}+d)', partial(res_b_xy_d, 1, xy)),
            0x96: (f'RES 2,({xy}+d)', partial(res_b_xy_d, 2, xy)),
            0x9E: (f'RES 3,({xy}+d)', partial(res_b_xy_d, 3, xy)),
            0xA6: (f'RES 4,({xy}+d)', partial(res_b_xy_d, 4, xy)),
            0xAE: (f'RES 5,({xy}+d)', partial(res_b_xy_d, 5, xy)),
            0xB6: (f'RES 6,({xy}+d)', partial(res_b_xy_d, 6, xy)),
            0xBE: (f'RES 7,({xy}+d)', partial(res_b_xy_d, 7, xy)),
            0xC6: (f'SET 0,({xy}+d)', partial(set_b_xy_d, 0, xy)),
            0xCE: (f'SET 1,({xy}+d)', partial(set_b_xy_d, 1, xy)),
            0xD6: (f'SET 2,({xy}+d)', partial(set_b_xy_d, 2, xy)),
            0xDE: (f'SET 3,({xy}+d)', partial(set_b_xy_d, 3, xy)),
            0xE6: (f'SET 4,({xy}+d)', partial(set_b_xy_d, 4, xy)),
            0xEE: (f'SET 5,({xy}+d)', partial(set_b_xy_d, 5, xy)),
            0xF6: (f'SET 6,({xy}+d)', partial(set_b_xy_d, 6, xy)),
            0xFE: (f'SET 7,({xy}+d)', partial(set_b_xy_d, 7, xy)),
        },
        0xE1: (f'POP {xy}',     partial(pop_qq, xy)),
        0xE3: (f'EX (SP),{xy}', partial(ex_psp_dd, xy)),
        0xE9: (f'JP ({xy})',    f'PC = {xy};'),
    }

# See https://wiki.specnext.dev/Extended_Z80_instruction_set.
instructions: Table = {
    0x00: ('NOP',       ''),
    0x01: ('LD BC,nn',  partial(ld_dd_nn, 'BC')),
    0x02: ('LD (BC),A', partial(ld_pdd_r, 'BC', 'A')),
    0x03: ('INC BC',    partial(inc_ss, 'BC')),
    0x04: ('INC B',     partial(inc_r, 'B')),
    0x05: ('DEC B',     partial(dec_r, 'B')),
    0x06: ('LD B,n',    partial(ld_r_n, 'B')),
    0x07: ('RLCA',
           '''
           const u8_t carry = A >> 7;
           F &= ~(NF_MASK | CF_MASK);
           F |= HF_MASK | carry << CF_SHIFT;
           A <<= 1;
           A |= carry;
           '''),
    0x08: ("EX AF,AF'", partial(ex, 'AF', 'AF_')),
    0x09: ('ADD HL,BC', partial(add_hl_ss, 'BC')),
    0x0A: ('LD A,(BC)', partial(ld_r_pdd, 'A', 'BC')),
    0x0B: ('DEC BC',    partial(dec_ss, 'BC')),
    0x0C: ('INC C',     partial(inc_r, 'C')),
    0x0D: ('DEC C',     partial(dec_r, 'C')),
    0x0E: ('LD C,n',    partial(ld_r_n, 'C')),
    0x0F: ('RRCA',
           '''
           const u8_t carry = A & 0x01;
           F &= ~(NF_MASK | HF_MASK | CF_MASK);
           F |= carry << CF_SHIFT;
           A >>= 1;
           A |= carry << 7;
           '''),
    0x10: ('DJNZ e',
           f'''
           T(1);
           Z = memory_read(PC++); T(3);
           if (--B) {{
             PC += (s8_t) Z; T(5);
           }}
           '''),
    0x11: ('LD DE,nn',  partial(ld_dd_nn, 'DE')),
    0x12: ('LD (DE),A', partial(ld_pdd_r, 'DE', 'A')),
    0x13: ('INC DE',    partial(inc_ss, 'DE')),
    0x14: ('INC D',     partial(inc_r, 'D')),
    0x15: ('DEC D',     partial(dec_r, 'D')),
    0x16: ('LD D,n',    partial(ld_r_n, 'D')),
    0x17: ('RLA',
           '''
           const u8_t carry = F & CF_MASK;
           F &= ~(HF_MASK | NF_MASK | CF_MASK);
           F |= (A & 0x80) >> 7 << CF_SHIFT;
           A <<= 1;
           A |= carry;
           '''),
    0x18: ('JR e',
           '''
           Z = memory_read(PC++); T(3);
           PC += (s8_t) Z;        T(5);
           '''),
    0x19: ('ADD HL,DE',  partial(add_hl_ss, 'DE')),
    0x1A: ('LD A,(DE)',  partial(ld_r_pdd, 'A', 'DE')),
    0x1B: ('DEC DE',     partial(dec_ss, 'DE')),
    0x1C: ('INC E',      partial(inc_r, 'E')),
    0x1D: ('DEC E',      partial(dec_r, 'E')),
    0x1E: ('LD E,n',     partial(ld_r_n, 'E')),
    0x1F: ('RRA',
           '''
           const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
           F &= ~(HF_MASK | NF_MASK | CF_MASK);
           F |= (A & 0x01) << CF_SHIFT;
           A >>= 1;
           A |= carry << 7;
           '''),
    0x20: ('JR NZ,e',    partial(jr_c_e, '!ZF')),
    0x21: ('LD HL,nn',   partial(ld_dd_nn, 'HL')),
    0x22: ('LD (nn),HL', partial(ld_pnn_dd, 'HL')),
    0x23: ('INC HL',     partial(inc_ss, 'HL')),
    0x24: ('INC H',      partial(inc_r, 'H')),
    0x25: ('DEC H',      partial(dec_r, 'H')),
    0x26: ('LD H,n',     partial(ld_r_n, 'H')),
    0x28: ('JR Z,e',     partial(jr_c_e, 'ZF')),
    0x29: ('ADD HL,HL',  partial(add_hl_ss, 'HL')),
    0x2A: ('LD HL,(nn)',
           '''
           Z = memory_read(PC++);   T(3);
           W = memory_read(PC++);   T(3);
           L = memory_read(WZ);     T(3);
           H = memory_read(WZ + 1); T(3);
           '''),
    0x2B: ('DEC HL',     partial(dec_ss, 'HL')),
    0x2C: ('INC L',      partial(inc_r, 'L')),
    0x2D: ('DEC L',      partial(dec_r, 'L')),
    0x2E: ('LD L,n',     partial(ld_r_n, 'L')),
    0x2F: ('CPL',
           '''
           A = ~A;
           F |= (1 << HF_SHIFT) | (1 << NF_SHIFT);
           '''),
    0x30: ('JR NC,e',   partial(jr_c_e, '!CF')),
    0x31: ('LD SP,nn',  partial(ld_dd_nn, 'SP')),
    0x32: ('LD nn,A',
           '''
           Z = memory_read(PC++); T(3);
           W = memory_read(PC++); T(3);
           memory_write(WZ, A);   T(3);
           '''),
    0x34: ('INC (HL)', inc_phl),
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
    0x38: ('JR C,e',    partial(jr_c_e, 'CF')),
    0x39: ('ADD HL,SP', partial(add_hl_ss, 'SP')),
    0x3A: ('LD A,(nn)',
           '''
           Z = memory_read(PC++); T(3);
           W = memory_read(PC++); T(3);
           A = memory_read(WZ);   T(3);
           '''),
    0x3C: ('INC A',     partial(inc_r, 'A')),
    0x3D: ('DEC A',     partial(dec_r, 'A')),
    0x3E: ('LD A,n',    partial(ld_r_n, 'A')),
    0x3F: ('CCF',
           '''
           const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
           F &= ~(HF_MASK | CF_MASK);
           F |= carry << HF_SHIFT | (1 - carry) << CF_SHIFT;
           '''),
    0x40: ('LD B,B',    partial(ld_r_r, 'B', 'B')),
    0x41: ('LD B,C',    partial(ld_r_r, 'B', 'C')),
    0x42: ('LD B,D',    partial(ld_r_r, 'B', 'D')),
    0x43: ('LD B,E',    partial(ld_r_r, 'B', 'E')),
    0x44: ('LD B,H',    partial(ld_r_r, 'B', 'H')),
    0x45: ('LD B,L',    partial(ld_r_r, 'B', 'L')),
    0x46: ('LD B,(HL)', partial(ld_r_pdd, 'B', 'HL')),
    0x47: ('LD B,A',    partial(ld_r_r, 'B', 'A')),
    0x48: ('LD C,B',    partial(ld_r_r, 'C', 'B')),
    0x49: ('LD C,C',    partial(ld_r_r, 'C', 'C')),
    0x4A: ('LD C,D',    partial(ld_r_r, 'C', 'D')),
    0x4B: ('LD C,E',    partial(ld_r_r, 'C', 'B')),
    0x4C: ('LD C,H',    partial(ld_r_r, 'C', 'H')),
    0x4D: ('LD C,L',    partial(ld_r_r, 'C', 'L')),
    0x4E: ('LD C,(HL)', partial(ld_r_pdd, 'C', 'HL')),
    0x4F: ('LD C,A',    partial(ld_r_r, 'C', 'A')),
    0x50: ('LD D,B',    partial(ld_r_r, 'D', 'B')),
    0x51: ('LD D,C',    partial(ld_r_r, 'D', 'C')),
    0x52: ('LD D,D',    partial(ld_r_r, 'D', 'D')),
    0x53: ('LD D,E',    partial(ld_r_r, 'D', 'E')),
    0x54: ('LD D,H',    partial(ld_r_r, 'D', 'H')),
    0x55: ('LD D,L',    partial(ld_r_r, 'D', 'L')),
    0x56: ('LD D,(HL)', partial(ld_r_pdd, 'D', 'HL')),
    0x57: ('LD D,A',    partial(ld_r_r, 'D', 'A')),
    0x58: ('LD E,B',    partial(ld_r_r, 'E', 'B')),
    0x59: ('LD E,C',    partial(ld_r_r, 'E', 'C')),
    0x5A: ('LD E,D',    partial(ld_r_r, 'E', 'D')),
    0x5B: ('LD E,E',    partial(ld_r_r, 'E', 'E')),
    0x5C: ('LD E,H',    partial(ld_r_r, 'E', 'H')),
    0x5D: ('LD E,L',    partial(ld_r_r, 'E', 'L')),
    0x5E: ('LD E,(HL)', partial(ld_r_pdd, 'E', 'HL')),
    0x5F: ('LD E,A',    partial(ld_r_r, 'E', 'A')),
    0x60: ('LD H,B',    partial(ld_r_r, 'H', 'B')),
    0x61: ('LD H,C',    partial(ld_r_r, 'H', 'C')),
    0x62: ('LD H,D',    partial(ld_r_r, 'H', 'D')),
    0x63: ('LD H,E',    partial(ld_r_r, 'H', 'E')),
    0x64: ('LD H,H',    partial(ld_r_r, 'H', 'H')),
    0x65: ('LD H,L',    partial(ld_r_r, 'H', 'L')),
    0x66: ('LD H,(HL)', partial(ld_r_pdd, 'H', 'HL')),
    0x67: ('LD H,A',    partial(ld_r_r, 'H', 'A')),
    0x68: ('LD L,B',    partial(ld_r_r, 'L', 'B')),
    0x69: ('LD L,C',    partial(ld_r_r, 'L', 'C')),
    0x6A: ('LD L,D',    partial(ld_r_r, 'L', 'D')),
    0x6B: ('LD L,E',    partial(ld_r_r, 'L', 'E')),
    0x6C: ('LD L,H',    partial(ld_r_r, 'L', 'H')),
    0x6D: ('LD L,L',    partial(ld_r_r, 'L', 'L')),
    0x6E: ('LD L,(HL)', partial(ld_r_pdd, 'L', 'HL')),
    0x6F: ('LD L,A',    partial(ld_r_r, 'L', 'A')),
    0x70: ('LD (HL),B', partial(ld_pdd_r, 'HL', 'B')),
    0x71: ('LD (HL),C', partial(ld_pdd_r, 'HL', 'C')),
    0x72: ('LD (HL),D', partial(ld_pdd_r, 'HL', 'D')),
    0x73: ('LD (HL),E', partial(ld_pdd_r, 'HL', 'E')),
    0x75: ('LD (HL),L', partial(ld_pdd_r, 'HL', 'L')),
    0x77: ('LD (HL),A', partial(ld_pdd_r, 'HL', 'A')),
    0x78: ('LD A,B',    partial(ld_r_r, 'A', 'B')),
    0x79: ('LD A,C',    partial(ld_r_r, 'A', 'C')),
    0x7A: ('LD A,D',    partial(ld_r_r, 'A', 'D')),
    0x7B: ('LD A,E',    partial(ld_r_r, 'A', 'E')),
    0x7C: ('LD A,H',    partial(ld_r_r, 'A', 'H')),
    0x7D: ('LD A,L',    partial(ld_r_r, 'A', 'L')),
    0x7E: ('LD A,(HL)', partial(ld_r_pdd, 'A', 'HL')),
    0x80: ('ADD A,B',   partial(add_a_r, 'B')),
    0x81: ('ADD A,C',   partial(add_a_r, 'C')),
    0x82: ('ADD A,D',   partial(add_a_r, 'D')),
    0x83: ('ADD A,E',   partial(add_a_r, 'E')),
    0x84: ('ADD A,H',   partial(add_a_r, 'H')),
    0x85: ('ADD A,L',   partial(add_a_r, 'L')),
    0x86: ('ADD A,(HL)', add_a_phl),
    0x87: ('ADD A,A',   partial(add_a_r, 'A')),
    0x88: ('ADC A,B',   partial(adc_a_r, 'B')),
    0x89: ('ADC A,C',   partial(adc_a_r, 'C')),
    0x8A: ('ADC A,D',   partial(adc_a_r, 'D')),
    0x8B: ('ADC A,E',   partial(adc_a_r, 'E')),
    0x8C: ('ADC A,H',   partial(adc_a_r, 'H')),
    0x8D: ('ADC A,L',   partial(adc_a_r, 'L')),
    0x8E: ('ADC A,(HL)', adc_a_phl),
    0x8F: ('ADC A,A',   partial(adc_a_r, 'A')),
    0x90: ('SUB B',     partial(sub_r, 'B')),
    0x91: ('SUB C',     partial(sub_r, 'C')),
    0x92: ('SUB D',     partial(sub_r, 'D')),
    0x93: ('SUB E',     partial(sub_r, 'E')),
    0x94: ('SUB H',     partial(sub_r, 'H')),
    0x95: ('SUB L',     partial(sub_r, 'L')),
    0x9F: ('SBC A,A',   partial(sbc_r, 'A')),
    0xA0: ('AND B',     partial(and_r, 'B')),
    0xA1: ('AND C',     partial(and_r, 'C')),
    0xA2: ('AND D',     partial(and_r, 'D')),
    0xA3: ('AND E',     partial(and_r, 'E')),
    0xA4: ('AND H',     partial(and_r, 'H')),
    0xA5: ('AND L',     partial(and_r, 'L')),
    0xA7: ('AND A',     partial(and_r, 'A')),
    0xA8: ('XOR B',     partial(xor_r, 'B')),
    0xA9: ('XOR C',     partial(xor_r, 'C')),
    0xAA: ('XOR D',     partial(xor_r, 'D')),
    0xAB: ('XOR E',     partial(xor_r, 'E')),
    0xAC: ('XOR H',     partial(xor_r, 'H')),
    0xAD: ('XOR L',     partial(xor_r, 'L')),
    0xAE: ('XOR (HL)',
           '''
           A ^= memory_read(HL); T(3);
           F = SZ53P(A);
           '''),
    0xAF: ('XOR A',     partial(xor_r, 'A')),
    0xB0: ('OR B',      partial(or_r, 'B')),
    0xB1: ('OR C',      partial(or_r, 'C')),
    0xB2: ('OR D',      partial(or_r, 'D')),
    0xB3: ('OR E',      partial(or_r, 'E')),
    0xB4: ('OR H',      partial(or_r, 'H')),
    0xB5: ('OR L',      partial(or_r, 'L')),
    0xB7: ('OR A',      partial(or_r, 'A')),
    0xB8: ('CP B',      partial(cp_r, 'B')),
    0xB9: ('CP C',      partial(cp_r, 'C')),
    0xBA: ('CP D',      partial(cp_r, 'D')),
    0xBB: ('CP E',      partial(cp_r, 'E')),
    0xBC: ('CP H',      partial(cp_r, 'H')),
    0xBD: ('CP L',      partial(cp_r, 'L')),
    0xBE: ('CP (HL)',
           '''
           const u8_t  n      = memory_read(HL); T(3);
           const u16_t result = A - n;
           F                  = SZ53(result & 0xFF) | HF_SUB(A, n, result) | VF_SUB(A, n, result) | NF_MASK | (A < n) << CF_SHIFT;
           '''),
    0xC0: ('RET NZ',    partial(ret, '!(F & ZF_MASK)')),
    0xC1: ('POP BC',    partial(pop_qq, 'BC')),
    0xC2: ('JP NZ,nn',  partial(jp, '!(F & ZF_MASK)')),
    0xC3: ('JP nn',     jp),
    0xC4: ('CALL NZ,nn', partial(call, '!(F & ZF_MASK)')),
    0xC5: ('PUSH BC',    partial(push_qq, 'BC')),
    0xC6: ('ADD A,n',
           '''
           const u8_t n       = memory_read(PC++); T(3);
           const u16_t result = A + n;
           const u8_t  carry  = A > 255 - n;
           F = SZ53(result & 0xFF) | HF_ADD(A, n, result) | VF_ADD(A, n, result) | carry;
           A = result & 0xFF;
           '''),
    0xC8: ('RET Z',      partial(ret, 'F & ZF_MASK')),
    0xC9: ('RET',        ret),
    0xCA: ('JP Z,nn',    partial(jp, 'F & ZF_MASK')),
    0xCB: cb_table(),
    0xCC: ('CALL Z,nn',  partial(call, 'F & ZF_MASK')),
    0xCD: ('CALL nn',    call),
    0xCE: ('ADC A,n',
           '''
           const u8_t  n      = memory_read(PC++); T(3);
           const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
           const u16_t result = A + n + carry;
           F = SZ53(A) | HF_ADD(A, n + carry, result) | VF_ADD(A, n + carry, result) | (result & 0x100) >> 8 << CF_SHIFT;
           A = result & 0xFF;
           '''),
    0xC7: ('RST $00',  partial(rst, 0x00)),
    0xCF: ('RST $08',  partial(rst, 0x08)),
    0xD0: ('RET NC',   partial(ret, '!(F & CF_MASK)')),
    0xD1: ('POP DE',   partial(pop_qq, 'DE')),
    0xD2: ('JP NC,nn', partial(jp, '!(F & CF_MASK)')),
    0xD3: ('OUT (n),A',
           '''
           WZ = A << 8 | memory_read(PC++); T(3);
           io_write(WZ, A);                 T(4);
           '''),
    0xD4: ('CALL NC,nn', partial(call, '!(F & CF_MASK)')),
    0xD5: ('PUSH DE',    partial(push_qq, 'DE')),
    0xD6: ('SUB n',
           '''
           u16_t result;
           Z      = memory_read(PC++); T(3);
           result = A - Z;
           F     = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | VF_SUB(A, Z, result) | NF_MASK | A < Z;
           A     = result & 0xFF;
           '''),
    0xD7: ('RST $10', partial(rst, 0x10)),
    0xD8: ('RET C',   partial(ret, 'F & CF_MASK')),
    0xD9: ('EXX',
           '''
           u16_t tmp;
           tmp = BC; BC = BC_; BC_ = tmp;
           tmp = DE; DE = DE_; DE_ = tmp;
           tmp = HL; HL = HL_; HL_ = tmp;
           '''),
    0xDA: ('JP C,nn', partial(jp, 'F & CF_MASK')),
    0xDB: ('IN A,(n)',
           '''
           Z = memory_read(PC++);   T(3);
           A = io_read(A << 8 | Z); T(4);
           F = SZ53P(A);
           '''),
    0xDC: ('CALL C,nn', partial(call, 'F & CF_MASK')),
    0xDD: xy_table('IX'),
    0xDF: ('RST $18', partial(rst, 0x18)),
    0xE1: ('POP HL',  partial(pop_qq, 'HL')),
    0xE3: ('EX (SP),HL', partial(ex_psp_dd, 'HL')),
    0xE5: ('PUSH HL', partial(push_qq, 'HL')),
    0xE6: ('AND n',
           '''
           A &= memory_read(PC++); T(3);
           F = SZ53P(A) | (1 << HF_SHIFT);
           '''),
    0xE7: ('RST $20', partial(rst, 0x20)),
    0xE8: ('RET PE',  partial(ret, 'F & PF_MASK')),
    0xE9: ('JP (HL)', 'PC = HL;'),
    0xEB: ('EX DE,HL', partial(ex, 'DE', 'HL')),
    0xEF: ('RST $28',  partial(rst, 0x28)),
    0xED: ed_table(),
    0xEE: ('XOR n',
           '''
           A ^= memory_read(PC++); T(3);
           F = SZ53P(A);
           '''),
    0xF1: ('POP AF',  partial(pop_qq, 'AF')),
    0xF2: ('JP P,nn', partial(jp, '!(F & SF_MASK)')),
    0xF3: ('DI', 'IFF1 = IFF2 = 0;'),
    0xF5: ('PUSH AF', partial(push_qq, 'AF')),
    0xF6: ('OR n',
           '''
           A |= memory_read(PC++); T(3);
           F = SZ53P(A);
           '''),
    0xF7: ('RST $30', partial(rst, 0x30)),
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


def make_disassembler(mnemonic: str) -> Tuple[str, int]:
    tokens = {
        'n'    : (2, lambda offset: f'memory_read(PC + {offset})'),
        'nn'   : (4, lambda offset: f'memory_read(PC + {offset} + 1) << 8 | memory_read(PC + {offset})'),
        'mm'   : (4, lambda offset: f'memory_read(PC + {offset}) << 8 | memory_read(PC + {offset} + 1)'),
        'e'    : (4, lambda offset: f'PC + {offset} + 1 + (s8_t) memory_read(PC + {offset})'),
        'd'    : (2, lambda offset: f'memory_read(PC + {offset})'),
        'reg'  : (2, lambda offset: f'memory_read(PC + {offset})'),
        'value': (2, lambda offset: f'memory_read(PC + {offset})'),
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

    return statement, offset


def make_dumper(prefix: List[Opcode], opcode: Opcode, length: int) -> str:
    s = 'fprintf(stderr, "'
    s += ' '.join(f'{p:02X}' for p in prefix + [opcode])
    s += ' %02X' * length
    for i in range(4 - len(prefix) - length):
        s += '   '
    s += '"'
    if length > 0:
        s += ', ' + ', '.join(f'memory_read(PC + {i})' for i in range(length))
    return s + ');'


def generate(instructions: Table, f: io.TextIOBase, prefix: Optional[List[Opcode]] = None) -> None:
    prefix         = prefix or []
    prefix_len     = len(prefix)
    prefix_str     = ''.join(f'${opcode:02X} ' for opcode in prefix)
    prefix_comment = f'/* {prefix_str}*/ ' if prefix else ''

    # Show on stderr the registers before and after each instruction execution,
    # as well as a disassembly of each executed instruction.
    debug = False

    if debug:
        if not prefix:
            f.write('''
fprintf(stderr, "     AF %04X BC %04X DE %04X HL %04X IX %04X IY %04X F %s%s-%s-%s%s%s\\n", AF, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
fprintf(stderr, "     AF'%04X BC'%04X DE'%04X HL'%04X PC %04X SP %04X I %02X\\n", AF_, BC_, DE_, HL_, PC, SP, I);
fprintf(stderr, "     ROM %d  DIVMMC %02X  PAGES %02X %02X %02X %02X %02X %02X %02X %02X\\n", mmu_rom_get(), divmmc_control_read(0xE3), mmu_page_get(0), mmu_page_get(1), mmu_page_get(2), mmu_page_get(3), mmu_page_get(4), mmu_page_get(5), mmu_page_get(6), mmu_page_get(7));
fprintf(stderr, "%04X ", PC);
''')

    if prefix == [0xDD, 0xCB] or prefix == [0xFD, 0xCB]:
        # Special opcode where 3rd byte is parameter and 4th byte needed for
        # decoding. Read 4th byte, but keep PC at 3rd byte.
        read_opcode = 'opcode = memory_read(PC + 1)'
    else:
        read_opcode = 'opcode = memory_read(PC++)'

    f.write(f'{read_opcode}; T(4);')
    if not prefix:
        f.write('R = (R & 0x80) | (++R & 0x7F);')
    f.write('switch (opcode) {')

    for opcode in sorted(instructions):
        item = instructions[opcode]
        if isinstance(item, tuple):
            mnemonic, spec       = item
            disassembler, length = make_disassembler(mnemonic)
            dumper               = make_dumper(prefix, opcode, length)
            c = spec() if callable(spec) else spec
            if c is not None:
                f.write(f'''
case {prefix_comment}0x{opcode:02X}:  /* {mnemonic} */
  {{
    {dumper       if debug else ''}
    {disassembler if debug else ''}
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
