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


# Helper routines.
def hi(dd: str) -> str:
    return f'{dd}H' if dd in ['IX', 'IY'] else dd[0]

def lo(dd: str) -> str:
    return f'{dd}L' if dd in ['IX', 'IY'] else dd[1]

def wz(xy: Optional[str] = None, rr: Optional[str] = 'HL') -> C:
    if rr == 'HL' and xy:
        return f'{xy} + (s8_t) memory_read(PC++); T(3 + 5)'
    else:
        return rr


def adc_A_n() -> C:
    return '''
        const u8_t  n      = memory_read(PC++); T(3);
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u16_t result = A + n + carry;
        F = SZ53(result & 0xFF) | HF_ADD(A, n + carry, result) | VF_ADD(A, n + carry, result) | (result > 0xFF) << CF_SHIFT;
        A = result & 0xFF;
    '''

def adc_A_r(r: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u16_t result = A + {r} + carry;
        F = SZ53(result & 0xFF) | HF_ADD(A, {r} + carry, result) | VF_ADD(A, {r} + carry, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A = result & 0xFF;
    '''

def adc_A_pss(xy: Optional[str] = None) -> C:
    return f'''
        const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
        u16_t      result;
        WZ     = {wz(xy)};
        TMP    = memory_read(WZ) + carry; T(3);
        result = A + TMP;
        F      = SZ53(result & 0xFF) | HF_ADD(A, TMP, result) | VF_ADD(A, TMP, result) | (result & 0x100) >> 8 << CF_SHIFT;
        A      = result & 0xFF;
        T(3);
    '''

    return None

def adc_HL_ss(ss: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u32_t result = HL + {ss} + carry; T(4);
        F  = (result & 0x8000) >> 15 << SF_SHIFT | (result == 0) << ZF_SHIFT | (result & 0x20) | (result & 0x08) | HF_ADD(H, {hi(ss)}, result >> 8) | VF_ADD(H, {hi(ss)}, result >> 8) | (result & 0x10000) >> 16 << CF_SHIFT;
        HL = result & 0xFFFF; T(3);
    '''

def add_A_n() -> C:
    return '''
        const u8_t n       = memory_read(PC++); T(3);
        const u16_t result = A + n;
        F = SZ53(result & 0xFF) | HF_ADD(A, n, result) | VF_ADD(A, n, result) | (result > 0xFF) << CF_SHIFT;
        A = result & 0xFF;
    '''

def add_A_r(r: str) -> C:
    return f'''
        const u16_t result = A + {r};
        const u8_t  carry  = A > 0xFF - {r};
        F = SZ53(result & 0xFF) | HF_ADD(A, {r}, result) | VF_ADD(A, {r}, result) | carry;
        A += {r} & 0xFF;
    '''

def add_A_pss(xy: Optional[str] = None) -> C:
    return f'''
        u16_t result;
        WZ     = {wz(xy)};
        TMP    = memory_read(WZ);
        result = A + TMP;
        F      = SZ53(result & 0xFF) | HF_ADD(A, TMP, result) | VF_ADD(A, TMP, result) | (result & 0x100) >> 8 << CF_SHIFT;
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

def add_dd_ss(dd: str, ss: str) -> C:
    return f'''
        const u16_t prev  = {dd};
        const u8_t  carry = {dd} > 0xFFFF - {ss};
        {dd} += {ss}; T(4 + 3);
        F &= ~(HF_MASK | NF_MASK | CF_MASK);
        F |= HF_ADD(prev >> 8, {ss} >> 8, {dd} >> 8) | carry;
    '''

def bit_b_r(b: int, r: str) -> C:
    return f'''
        F &= ~(ZF_MASK | NF_MASK);
        F |= ({r} & 1 << {b} ? 0 : ZF_MASK) | HF_MASK;
        T(4);
    '''

def bit_b_pss(b: int, xy: Optional[str] = None) -> C:
    return f'''
        WZ = {wz(xy)};
        F &= ~(ZF_MASK | NF_MASK);
        F |= (memory_read(WZ) & 1 << {b} ? 0 : ZF_MASK) | HF_MASK; T(4);
        T(4);
    '''

def brlc() -> C:
    return '''
        DE = (DE << (B & 0x0F))
           | (DE >> (0x10 - B & 0x0F));
    '''

def bsla() -> C:
    return 'DE <<= B & 0x1F;'

def bsra() -> C:
    return 'DE = (u16_t) ((s16_t) DE >> (B & 0x1F));'

def bsrf() -> C:
    return 'DE = ~(((u16_t) ~DE) >> (B & 0x1F));'

def bsrl() -> C:
    return 'DE >>= B & 0x1F;'

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

def ccf() -> C:
    return '''
        const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
        F &= ~(HF_MASK | CF_MASK);
        F |= carry << HF_SHIFT | (1 - carry) << CF_SHIFT;
    '''

def cp_n() -> C:
    return '''
        u16_t result;
        TMP    = memory_read(PC++); T(3);
        result = A - TMP;
        F      = SZ53(result & 0xFF) | HF_SUB(A, TMP, result) | VF_SUB(A, TMP, result) | NF_MASK | (A < TMP) << CF_SHIFT;
    '''

def cp_r(r: str) -> C:
    return f'''
        const u16_t result = A - {r};
        F                  = SZ53(result & 0xFF) | HF_SUB(A, {r}, result) | VF_SUB(A, {r}, result) | NF_MASK | (A < {r}) << CF_SHIFT;
    '''

def cp_pss(xy: Optional[str] = None) -> C:
    return f'''
        u16_t result;
        WZ     = {wz(xy)};
        TMP    = memory_read(WZ); T(3);
        result = A - TMP;
        F      = SZ53(result & 0xFF) | HF_SUB(A, TMP, result) | VF_SUB(A, TMP, result) | NF_MASK | (A < TMP) << CF_SHIFT;
    '''

def cpx(op: str) -> C:
    return f'''
      u16_t result;
      Z      = memory_read(HL{op}{op}); T(3);
      result = A - Z;
      --BC;
      F      = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | (BC != 0) << VF_SHIFT | NF_MASK | (F & CF_MASK);
      T(5);
    '''
    
def cpxr(op: str) -> C:
    return f'''
      u16_t result;
      Z      = memory_read(HL{op}{op}); T(3);
      result = A - Z;
      --BC; R++; T(5);
      if (!(BC == 0 || result == 0)) {{
          PC -= 2; T(5);
      }} else {{
          F = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | (BC != 0) << VF_SHIFT | NF_MASK | (F & CF_MASK);
      }}
    '''

def cpl() -> C:
    return '''
        A = ~A;
        F |= HF_MASK | NF_MASK;
    '''

def daa() -> C:
    # Credits to https://www.msx.org/forum/semi-msx-talk/emulation/z80-daa.
    return '''
        u8_t a = A;
        if (F & NF_MASK) {
            if ((F & HF_MASK) || (A & 0x0F) > 9) A -= 6;
            if ((F & CF_MASK) || A > 0x99)       A -= 0x60;
        } else {
            if ((F & HF_MASK) || (A & 0x0F) > 9) A += 6;
            if ((F & CF_MASK) || A > 0x99)       A += 0x60;
        }
        
        F = SZ53P(A) | ((a ^ A) & 0x10) >> 4 << HF_SHIFT | (F & (NF_MASK | CF_MASK)) | (a > 0x99) << CF_SHIFT;
    '''

def dec_pdd(xy: Optional[str] = None) -> C:
    return f'''
        WZ  = {wz(xy)};
        TMP = memory_read(WZ) - 1; T(4);
        memory_write(WZ, TMP);     T(3);
        F = SZ53(TMP) | HF_SUB(TMP + 1, 1, TMP) | (TMP == 0x79) << VF_SHIFT | NF_MASK | (F & CF_MASK);
    '''

def dec_r(r: str) -> C:
    return f'''
        {r}--;
        F = SZ53({r}) | HF_SUB({r} + 1, 1, {r}) | ({r} == 0x79) << VF_SHIFT | NF_MASK | (F & CF_MASK);
    '''

def dec_ss(ss: str) -> C:
    return f'{ss}--; T(2);'

def di() -> C:
    return 'IFF1 = IFF2 = 0;'

def djnz() -> C:
    return f'''
        T(1);
        Z = memory_read(PC++); T(3);
        if (--B) {{
            PC += (s8_t) Z; T(5);
        }}
    '''

def ei() -> C:
    return '''
        IFF1 = IFF2 = 1;
        self.irq_delay = 1;
    '''

def ex(r1: str, r2: str) -> C:
    return f'''
        const u16_t tmp = {r1};
        {r1} = {r2};
        {r2} = tmp;
    '''

def ex_pSP_dd(dd: str) -> C:
    return f'''
        Z = memory_read(SP);            T(3);
        W = memory_read(SP + 1);        T(3);
        memory_write(SP,     {lo(dd)}); T(4);
        memory_write(SP + 1, {hi(dd)}); T(5);
        {hi(dd)} = W;
        {lo(dd)} = Z;
    '''

def exx() -> C:
    return '''
        u16_t tmp;
        tmp = BC; BC = BC_; BC_ = tmp;
        tmp = DE; DE = DE_; DE_ = tmp;
        tmp = HL; HL = HL_; HL_ = tmp;
    '''

def halt() -> C:
    return '''
        if (!self.irq_pending || IFF1 == 0) {
          PC--;
        }
    '''

def im(mode: int) -> C:
    return f'''
        IM = {mode};
    '''

def in_A_n() -> C:
    return '''
        Z = memory_read(PC++);   T(3);
        A = io_read(A << 8 | Z); T(4);
        F = SZ53P(A);
    '''

def in_r_C(r: str) -> C:
    return f'''
        {r} = io_read(BC); T(4);
        F = SZ53P({r});
    '''

def inc_pdd(xy: Optional[str] = None) -> C:
    return f'''
        WZ  = {wz(xy)};
        TMP = memory_read(WZ) + 1; T(4);
        memory_write(WZ, TMP);     T(3);
        F = SZ53(TMP) | HF_ADD(TMP - 1, 1, TMP) | (TMP == 0x80) << VF_SHIFT | (F & CF_MASK);
    '''

def inc_r(r: str) -> C:
    return f'''
       {r}++;
       F = SZ53({r}) | HF_ADD({r} - 1, 1, {r}) | ({r} == 0x80) << VF_SHIFT | (F & CF_MASK);
    '''

def inc_ss(ss: str) -> C:
    return f'{ss}++; T(2);'

def inx(op: str) -> C:
    return f'''
        T(1);
        Z = io_read(BC);             T(3);
        memory_write(HL{op}{op}, Z); T(4);
        --B;
        F = SZ53P(B) | NF_MASK | (F & CF_MASK);
    '''

def inxr(op: str) -> C:
    return f'''
        T(1);
        Z = io_read(BC);             T(3);
        memory_write(HL{op}{op}, Z); T(4);
        if (--B) {{
            PC -= 2; T(5);
            R++;
        }} else {{
            F |= ZF_MASK | NF_MASK;
        }}
    '''

def jr_c_e(cond: Optional[str] = None) -> C:
    s = '''
        Z = memory_read(PC++); T(3);
    '''

    if cond:
        s += f'if ({cond}) {{\n'
    
    s += 'PC += (s8_t) Z; T(5);'

    if cond:
        s += '}'

    return s

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

def jp_pss(ss: str) -> C:
    return f'PC = {ss};'

def jpc() -> C:
    return '''
        PC = PC & 0xC000 + (io_read(BC) << 6);
        T(5);
    '''

def ld_A_I() -> C:
    return '''
        T(1);
        A = I;
        F = SZ53(A) | (IFF2 << PF_SHIFT) | (F & CF_MASK);
    '''

def ld_A_pnn() -> C:
    return '''
        Z = memory_read(PC++); T(3);
        W = memory_read(PC++); T(3);
        A = memory_read(WZ);   T(3);
    '''

def ld_A_R() -> C:
    return '''
        T(1);
        A = R;
        F = SZ53(A) | (IFF2 << VF_SHIFT) | (F & CF_MASK) >> CF_SHIFT;
    '''

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

def ld_dd_ss(dd: str, ss: str) -> C:
    return f'''
        {dd} = {ss}; T(2);
    '''

def ld_I_A() -> C:
    return '''
        T(1);
        I = A;
    '''

def ld_pdd_r(dd: str, r: str, xy: Optional[str] = None) -> C:
    return f'''
        WZ = {wz(xy, dd)};
        memory_write(WZ, {r}); T(3);
    '''

def ld_pss_n(xy: Optional[str] = None) -> C:
    return f'''
        WZ  = {wz(xy)};
        TMP = memory_read(PC++); T(3);
        memory_write(WZ, TMP);   T(3);
    '''

def ld_pnn_a() -> C:
    return '''
        Z = memory_read(PC++); T(3);
        W = memory_read(PC++); T(3);
        memory_write(WZ, A);   T(3);
    '''

def ld_pnn_dd(dd: str) -> C:
    return f'''
        Z = memory_read(PC++);          T(3);
        W = memory_read(PC++);          T(3);
        memory_write(WZ,     {lo(dd)}); T(3);
        memory_write(WZ + 1, {hi(dd)}); T(3);
    '''

def ld_R_A() -> C:
    return '''
        T(1);
        R = A;
    '''
    
def ld_r_pdd(r: str, dd: str, xy: Optional[str] = None) -> C:
    return f'''
        WZ = {wz(xy, dd)};
        {r} = memory_read(WZ); T(3);
    '''

def ld_r_n(r: str) -> C:
    return f'{r} = memory_read(PC++); T(3);'

def ld_r_r(r1: str, r2: str) -> C:
    return f'{r1} = {r2};'

def ldws() -> C:
    return '''
        TMP = memory_read(DE); T(3);
        memory_write(HL, TMP); T(3);
        L++; D++;
        F = SZ53(D) | HF_ADD(D - 1, 1, D) | (D == 0x80) << VF_SHIFT | (F & CF_MASK);
    '''

def ldx(op: str) -> C:
    return f'''
        TMP = memory_read(HL{op}{op}); T(3);
        memory_write(DE{op}{op}, TMP); T(5);
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (--BC != 0) << VF_SHIFT;
    '''

def ldxr(op: str) -> C:
    return f'''
        TMP  = memory_read(HL{op}{op}); T(3);
        memory_write(DE{op}{op}, TMP);  T(5);
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (--BC != 0) << VF_SHIFT;
        if (BC) {{
            PC -= 2; T(5);
        }}
    '''

def ldxx(op: str) -> C:
    return f'''
        TMP = memory_read(HL{op}{op}); T(3);
        if (TMP != A) {{
            memory_write(DE, TMP);     T(5);
        }}
        DE{op}{op};
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (--BC != 0) << VF_SHIFT;
    '''

def logical_n(op: str) -> C:
    return f'''
        A {op}= memory_read(PC++); T(3);
        F = SZ53P(A) | {"HF_MASK" if op == '&' else 0};
    '''

def logical_r(op: str, r: str) -> C:
    return f'''
       A {op}= {r};
       F = SZ53P(A) | {"HF_MASK" if op == '&' else 0};
    '''

def logical_pss(op: str, xy: Optional[str] = None) -> C:
    return f'''
        WZ    = {wz(xy)};
        A {op}= memory_read(WZ);                            T(4);
        F     = SZ53P(A) | {"HF_MASK" if op == '&' else 0}; T(3);
    '''

def lxrx(op: str) -> C:
    return f'''
        TMP = memory_read(HL{op}{op}); T(3);
        if (TMP != A) {{
            memory_write(DE, TMP);     T(5);
        }}
        DE{op}{op};
        F &= ~(HF_MASK | VF_MASK | NF_MASK);
        F |= (--BC != 0) << VF_SHIFT;
        if (BC) {{
          PC -= 2; T(5);
        }}
    '''

def mirr() -> C:
    return '''
        A = (A & 0x01) << 7
          | (A & 0x02) << 5
          | (A & 0x04) << 3
          | (A & 0x08) << 1
          | (A & 0x10) >> 1
          | (A & 0x20) >> 3
          | (A & 0x40) >> 5
          | (A & 0x80) >> 7;
    '''

def mul() -> C:
    return 'DE = D * E;'

def neg() -> C:
    return '''
        const u8_t prev = A;
        A = -A;
        F = SZ53(A) | HF_SUB(0, prev, A) | (prev == 0x80) << VF_SHIFT | NF_MASK | (prev != 0x00) << CF_SHIFT;
    '''

def nop() -> C:
    return ''

def nreg_reg_A() -> C:
    return '''
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, A);
        T(4);
    '''

def nreg_reg_value() -> C:
    return '''
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, memory_read(PC++));
        T(8);
    '''

def otib() -> C:
    return '''
        TMP = memory_read(HL++); T(4);
        io_write(BC, TMP);       T(4);
    '''
    
def otxr(op: str) -> C:
    return f'''
        T(1);
        Z = memory_read(HL{op}{op}); T(3);
        --B;
        io_write(BC, Z);             T(4);
        if (B) {{
            PC -= 2; T(5);
            R++;
        }} else {{
            F |= ZF_MASK | NF_MASK;
        }}
    '''

def out_C_r(r: str) -> C:
    return f'io_write(BC, {r}); T(4);'

def out_n_a() -> C:
    return '''
        WZ = A << 8 | memory_read(PC++); T(3);
        io_write(WZ, A);                 T(4);
    '''

def outx(op: str) -> C:
    return f'''
        T(1);
        Z = memory_read(HL{op}{op}); T(3);
        --B;
        io_write(BC, Z);             T(4);
        F = SZ53P(B) | NF_MASK | (F & CF_MASK);    
    '''

def pop_qq(qq: str) -> C:
    return f'''
        {lo(qq)} = memory_read(SP++); T(3);
        {hi(qq)} = memory_read(SP++); T(3);
    '''

def push_im() -> C:
    return '''
        W = memory_read(PC++);  /* High byte first. */
        Z = memory_read(PC++);  /* Then low byte.   */
        memory_write(--SP, W);
        memory_write(--SP, Z);
        T(15);
    '''

def push_qq(qq: str) -> C:
    return f'''
        T(1);
        memory_write(--SP, {hi(qq)}); T(3);
        memory_write(--SP, {lo(qq)}); T(3);
    '''

def pxad() -> C:
    return '''
        HL = 0x4000 + ((D & 0xC0) << 5) + ((D & 0x07) << 8) + ((D & 0x38) << 2) + (E >> 3);
    '''

def pxdn() -> C:
    return '''
        if ((HL & 0x0700) != 0x0700) {
            HL += 256;
        } else if ((HL & 0xE0) != 0xE0) {
            HL = HL & 0xF8FF + 0x20;
        } else {
           HL = HL & 0xF81F + 0x0800;
        }
    '''

def res_b_pss(b: int, xy: Optional[str] = None) -> C:
    return f'''
        WZ = {wz(xy)};
        memory_write(WZ, memory_read(WZ) & ~(1 << {b})); T(4 + 3);
    '''

def res_b_r(b: int, r: str) -> C:
    return f'{r} &= ~(1 << {b});'

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

def reti() -> C:
    return '''
        PCL = memory_read(SP++); T(3);
        PCH = memory_read(SP++); T(3);
    '''    

def retn() -> C:
    return '''
        PCL = memory_read(SP++); T(3);
        PCH = memory_read(SP++); T(3);
        IFF1 = IFF2;
    '''

def rl_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP >> 7;
        TMP   = TMP << 1 | (F & CF_MASK) >> CF_SHIFT;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def rl_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} = {r} << 1 | (F & CF_MASK) >> CF_SHIFT;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rla() -> C:
    return '''
        const u8_t carry = F & CF_MASK;
        F &= ~(HF_MASK | NF_MASK | CF_MASK);
        F |= (A & 0x80) >> 7 << CF_SHIFT;
        A <<= 1;
        A |= carry;
    '''

def rlc_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP >> 7;
        TMP   = TMP << 1 | carry;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def rlc_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} = {r} << 1 | carry;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rlca() -> C:
    return '''
        const u8_t carry = A >> 7;
        F &= ~(NF_MASK | CF_MASK);
        F |= HF_MASK | carry << CF_SHIFT;
        A <<= 1;
        A |= carry;
    '''

def rld() -> C:
    return '''
        Z = memory_read(HL); T(3);
        memory_write(HL, Z << 4 | (A & 0x0F)); T(4);
        A = (A & 0xF0) | Z >> 4;
        F = SZ53P(A) | (F & CF_MASK);
        T(3);
    '''

def rr_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP & 0x01;
        TMP   = TMP >> 1 | (F & CF_MASK) >> CF_SHIFT << 7;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def rr_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = {r} >> 1 | (F & CF_MASK) >> CF_SHIFT << 7;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rra() -> C:
    return '''
        const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
        F &= ~(HF_MASK | NF_MASK | CF_MASK);
        F |= (A & 0x01) << CF_SHIFT;
        A >>= 1;
        A |= carry << 7;
    '''

def rrc_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP & 0x01;
        TMP   = TMP >> 1 | carry << 7;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def rrc_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = {r} >> 1 | carry << 7;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def rrca() -> C:
    return '''
        const u8_t carry = A & 0x01;
        F &= ~(NF_MASK | HF_MASK | CF_MASK);
        F |= carry << CF_SHIFT;
        A >>= 1;
        A |= carry << 7;
    '''

def rrd() -> C:
    return '''
        Z = memory_read(HL); T(3);
        memory_write(HL, (A & 0x0F) << 4 | Z >> 4); T(4);
        A = (A & 0xF0) | (Z & 0x0F);
        F = SZ53P(A) | (F & CF_MASK);
        T(3);
    '''

def rst(address: int) -> C:
    return f'''
        T(1);
        memory_write(--SP, PCH); T(3);
        memory_write(--SP, PCL);
        PC = 0x{address:02X};    T(3);
    '''

def sbc_A_n() -> C:
    return '''
        const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
        const u8_t a     = A;
        s16_t      result;
        TMP    = memory_read(PC++) + carry; T(3);
        result = A - TMP;
        A      = result & 0xFF;
        F  = SZ53(A) | HF_SUB(a, TMP, A) | VF_SUB(a, TMP, A) | NF_MASK | (result < 0) << CF_SHIFT;
    '''

def sbc_A_pss(xy: Optional[str] = None) -> C:
    return f'''
        const u8_t  carry = (F & CF_MASK) >> CF_SHIFT;
        const u8_t  a     = A;
        s16_t       result;
        WZ     = {wz(xy)};
        TMP    = memory_read(WZ) + carry; T(3);
        result = A - TMP;
        A      = result & 0xFF;
        F      = SZ53(A) | HF_SUB(a, TMP, A) | VF_SUB(a, TMP, A) | NF_MASK | (result < 0) << CF_SHIFT;
    '''

def sbc_A_r(r: str) -> C:
    return f'''
        const u8_t carry = (F & CF_MASK) >> CF_SHIFT;
        const u8_t a     = A;
        s16_t      result;
        Z      = {r} + carry;
        result = A - Z;
        A      = result & 0xFF;
        F      = SZ53(A) | HF_SUB(a, Z, A) | VF_SUB(a, Z, A) | NF_MASK | (result < 0) << CF_SHIFT;
    '''

def sbc_HL_ss(ss: str) -> C:
    return f'''
        const u8_t  carry  = (F & CF_MASK) >> CF_SHIFT;
        const u32_t result = HL - {ss} - carry; T(4);
        F  = (result & 0x8000) >> 15 << SF_SHIFT | (result == 0) << ZF_SHIFT | (result & 0x20) | (result & 0x08) | HF_SUB(H, {hi(ss)}, result >> 8) | VF_SUB(H, {hi(ss)}, result >> 8) | NF_MASK | (HL < {ss} + carry) << CF_SHIFT;
        HL = result & 0xFFFF; T(3);
    '''

def scf() -> C:
    return '''
        F &= ~(HF_MASK | NF_MASK);
        F |= CF_MASK;
    '''

def set_b_pss(b: int, xy: Optional[str] = None) -> C:
    return f'''
      WZ = {wz(xy)};
      memory_write(WZ, memory_read(WZ) | 1 << {b}); T(4 + 3);
    '''

def set_b_r(b: int, r: str) -> C:
    return f'{r} |= 1 << {b};'

def stae() -> C:
    return f'A = (unsigned char) 0x80 >> (E & 7);'

def sla_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP >> 7;
        TMP <<= 1;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def sla_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} >> 7;
        {r} <<= 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def sra_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP & 0x01;
        TMP   = (TMP & 0x80) | TMP >> 1;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def sra_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} = ({r} & 0x80) | {r} >> 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def srl_pss(xy: Optional[str] = None) -> C:
    return f'''
        u8_t carry;
        WZ    = {wz(xy)};
        TMP   = memory_read(WZ);
        carry = TMP & 0x01;
        TMP >>= 1;
        F     = SZ53P(TMP) | carry << CF_SHIFT;
        T(4);
        memory_write(WZ, TMP);
        T(3);
    '''

def srl_r(r: str) -> C:
    return f'''
        const u8_t carry = {r} & 0x01;
        {r} >>= 1;
        F = SZ53P({r}) | carry << CF_SHIFT;
    '''

def sub_n() -> C:
    return '''
        u16_t result;
        Z      = memory_read(PC++); T(3);
        result = A - Z;
        F     = SZ53(result & 0xFF) | HF_SUB(A, Z, result) | VF_SUB(A, Z, result) | NF_MASK | A < Z;
        A     = result & 0xFF;
    '''

def sub_r(r: str) -> C:
    return f'''
        const u8_t  a      = A;
        const s16_t result = A - {r};
        A                  = result & 0xFF;
        F                  = SZ53(A) | HF_SUB(a, {r}, A) | VF_SUB(a, {r}, A) | NF_MASK | (result < 0) << CF_SHIFT;
    '''

def sub_pss(xy: Optional[str] = None) -> C:
    return f'''
        const u8_t a = A;
        s16_t      result;
        WZ     = {wz(xy)};
        TMP    = memory_read(WZ); T(3);
        result = A - TMP;
        A      = result & 0xFF;
        F      = SZ53(A) | HF_SUB(a, TMP, A) | VF_SUB(a, TMP, A) | NF_MASK | (result < 0) << CF_SHIFT;
    '''

def swap() -> C:
    return '''
        A = A >> 4 | A << 4;
        T(8);
    '''

def test() -> C:
    return '''
       TMP = A & memory_read(PC++); T(3);
       F   = SZ53P(TMP) | HF_MASK;
    '''


def cb_table(xy: Optional[str] = None) -> Table:
    hld = f'{xy}+d' if xy else 'HL'
    t   = {}

    def _bit_twiddling(mnemonic: str, B_opcode: int, act_b_r: Callable[[int, str], C], act_b_pss: Callable[[int, str], C]) -> None:
        for top_opcode, r in zip(range(B_opcode, B_opcode + 6), 'BCDEHL'):
            for opcode, b in zip(count(top_opcode, 8), range(8)):
                t[opcode] = (f'{mnemonic} {b},{r}', partial(act_b_r, b, r))

        for opcode, b in zip(count(B_opcode + 6, 8), range(8)):
            t[opcode] = (f'{mnemonic} {b},({hld})', partial(act_b_pss, b, xy))

        for opcode, b in zip(count(B_opcode + 7, 8), range(8)):
            t[opcode] = (f'{mnemonic} {b},A', partial(act_b_r, b, 'A'))

    def _rotates(B_opcode: int) -> None:
        actions = [
            ('RLC', rlc_r, rlc_pss),
            ('RRC', rrc_r, rrc_pss),
            ('RL',  rl_r,  rl_pss),
            ('RR',  rr_r,  rr_pss),
            ('SLA', sla_r, sla_pss),
            ('SRA', sra_r, sra_pss),
            ('SRL', srl_r, srl_pss)
        ]

        for top_opcode, r in zip(range(B_opcode, B_opcode + 6), 'BCDEHL'):
            deltas = [0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x38]
            for delta, (mnemonic, act_r, act_pss) in zip(deltas, actions):
                t[top_opcode + delta] = (f'{mnemonic} {r}', partial(act_r, r))

        for delta, (mnemonic, act_r, act_pss) in zip(deltas, actions):
            t[B_opcode + 6 + delta] = (f'{mnemonic} ({hld})', partial(act_pss, xy))
            t[B_opcode + 7 + delta] = (f'{mnemonic} A',       partial(act_r, 'A'))

    _bit_twiddling('BIT', 0x40, bit_b_r, bit_b_pss)
    _bit_twiddling('RES', 0x80, res_b_r, res_b_pss)
    _bit_twiddling('SET', 0xC0, set_b_r, set_b_pss)

    _rotates(0x00)

    return t


# See https://wiki.specnext.dev/Extended_Z80_instruction_set.
def ed_table() -> Table:
    return {
        0x23: ('SWAP',           swap),
        0x24: ('MIRR A',         mirr),
        0x27: ('TEST n',         test),
        0x28: ('BSLA DE,B',      bsla),
        0x29: ('BSRA DE,B',      bsra),
        0x2A: ('BSRL DE,B',      bsrl),
        0x2B: ('BSRF DE,B',      bsrf),
        0x2C: ('BRLC DE,B',      brlc),
        0x30: ('MUL D,E',        mul),
        0x31: ('ADD HL,A',       partial(add_dd_r, 'HL', 'A')),
        0x32: ('ADD DE,A',       partial(add_dd_r, 'DE', 'A')),
        0x33: ('ADD BC,A',       partial(add_dd_r, 'BC', 'A')),
        0x34: ('ADD HL,nn',      partial(add_dd_nn, 'HL')),
        0x35: ('ADD DE,nn',      partial(add_dd_nn, 'DE')),
        0x36: ('ADD BC,nn',      partial(add_dd_nn, 'DE')),
        0x40: ('IN B,(C)',       partial(in_r_C,  'B')),
        0x41: ('OUT (C),B',      partial(out_C_r, 'B')),
        0x42: ('SBC HL,BC',      partial(sbc_HL_ss, 'BC')),
        0x43: ('LD (nn),BC',     partial(ld_pnn_dd, 'BC')),
        0x44: ('NEG',            neg),
        0x45: ('RETN',           retn),
        0x46: ('IM 0',           partial(im, 0)),
        0x47: ('LD I,A',         ld_I_A),
        0x48: ('IN C,(C)',       partial(in_r_C,  'C')),
        0x49: ('OUT (C),C',      partial(out_C_r, 'C')),
        0x4A: ('ADC HL,BC',      partial(adc_HL_ss, 'BC')),
        0x4B: ('LD BC,(nn)',     partial(ld_dd_pnn, 'BC')),
        0x4D: ('RETI',           reti),
        0x4F: ('LD R,A',         ld_R_A),
        0x50: ('IN D,(C)',       partial(in_r_C, 'D')),
        0x51: ('OUT (C),D',      partial(out_C_r, 'D')),
        0x52: ('SBC HL,DE',      partial(sbc_HL_ss, 'DE')),
        0x53: ('LD (nn),DE',     partial(ld_pnn_dd, 'DE')),
        0x56: ('IM 1',           partial(im, 1)),
        0x57: ('LD A,I',         ld_A_I),
        0x58: ('IN E,(C)',       partial(in_r_C,  'E')),
        0x59: ('OUT (C),E',      partial(out_C_r, 'E')),
        0x5A: ('ADC HL,DE',      partial(adc_HL_ss, 'DE')),
        0x5B: ('LD DE,(nn)',     partial(ld_dd_pnn, 'DE')),
        0x5E: ('IM 2',           partial(im, 2)),
        0x5F: ('LD A,R',         ld_A_R),
        0x60: ('IN H,(C)',       partial(in_r_C,  'H')),
        0x61: ('OUT (C),H',      partial(out_C_r, 'H')),
        0x62: ('SBC HL,HL',      partial(sbc_HL_ss, 'HL')),
        0x67: ('RRD',            rrd),
        0x68: ('IN L,(C)',       partial(in_r_C, 'L')),
        0x69: ('OUT (C),L',      partial(out_C_r, 'L')),
        0x6A: ('ADC HL,HL',      partial(adc_HL_ss, 'HL')),
        0x6F: ('RLD',            rld),
        0x72: ('SBC HL,SP',      partial(sbc_HL_ss, 'SP')),
        0x73: ('LD (nn),SP',     partial(ld_pnn_dd, 'SP')),
        0x78: ('IN A,(C)',       partial(in_r_C, 'A')),
        0x79: ('OUT (C),A',      partial(out_C_r, 'A')),
        0x7A: ('ADC HL,SP',      partial(adc_HL_ss, 'SP')),
        0x7B: ('LD SP,(nn)',     partial(ld_dd_pnn, 'SP')),
        0x8A: ('PUSH mm',        push_im),
        0x90: ('OTIB',           otib),
        0x91: ('NREG reg,value', nreg_reg_value),
        0x92: ('NREG reg,A',     nreg_reg_A),
        0x93: ('PXDN',           pxdn),
        0x94: ('PXAD',           pxad),
        0x95: ('STAE',           stae),
        0x98: ('JP (C)',         jpc),
        0xA0: ('LDI',            partial(ldx,  '+')),
        0xA1: ('CPI',            partial(cpx,  '+')),
        0xA2: ('INI',            partial(inx,  '+')),
        0xA3: ('OUTI',           partial(outx, '+')),
        0xA4: ('LDIX',           partial(ldxx, '+')),
        0xA5: ('LDWS',           ldws),
        0xA8: ('LDD',            partial(ldx,  '-')),
        0xA9: ('CPD',            partial(cpx,  '-')),
        0xAA: ('IND',            partial(inx,  '-')),
        0xAB: ('OUTD',           partial(outx, '-')),
        0xAC: ('LDDX',           partial(ldxx, '-')),
        0xB0: ('LDIR',           partial(ldxr, '+')),
        0xB1: ('CPIR',           partial(cpxr, '+')),
        0xB2: ('INIR',           partial(inxr, '+')),
        0xB3: ('OTIR',           partial(otxr, '+')),
        0xB4: ('LIRX',           partial(lxrx, '+')),
        0xB8: ('LDDR',           partial(ldxr, '-')),
        0xB9: ('CPDR',           partial(cpxr, '-')),
        0xBA: ('INDR',           partial(inxr, '-')),
        0xBB: ('OTDR',           partial(otxr, '-')),
        0xBC: ('LDRX',           partial(lxrx, '-')),
    }


def table(xy: Optional[str] = None) -> Table:
    if xy:
        hl   = xy
        hld  = f'{xy}+d'
    else:
        hl   = 'HL'
        hld  = 'HL'

    h, l = hi(hl), lo(hl)

    t = {
        # 1x: complete.
        0x00: (f'NOP',          nop),
        0x01: (f'LD BC,nn',     partial(ld_dd_nn, 'BC')),
        0x02: (f'LD (BC),A',    partial(ld_pdd_r, 'BC', 'A')),
        0x03: (f'INC BC',       partial(inc_ss,   'BC')),
        0x04: (f'INC B',        partial(inc_r,    'B')),
        0x05: (f'DEC B',        partial(dec_r,    'B')),
        0x06: (f'LD B,n',       partial(ld_r_n,   'B')),
        0x07: (f'RLCA',         rlca),
        0x08: (f"EX AF,AF'",    partial(ex,        'AF', 'AF_')),
        0x09: (f'ADD {hl},BC',  partial(add_dd_ss, hl,   'BC')),
        0x0A: (f'LD A,(BC)',    partial(ld_r_pdd,  'A',  'BC')),
        0x0B: (f'DEC BC',       partial(dec_ss,    'BC')),
        0x0C: (f'INC C',        partial(inc_r,     'C')),
        0x0D: (f'DEC C',        partial(dec_r,     'C')),
        0x0E: (f'LD C,n',       partial(ld_r_n,    'C')),
        0x0F: (f'RRCA',         rrca),

        # 1x: complete.
        0x10: (f'DJNZ e',      djnz),
        0x11: (f'LD DE,nn',    partial(ld_dd_nn, 'DE')),
        0x12: (f'LD (DE),A',   partial(ld_pdd_r, 'DE', 'A')),
        0x13: (f'INC DE',      partial(inc_ss,   'DE')),
        0x14: (f'INC D',       partial(inc_r,    'D')),
        0x15: (f'DEC D',       partial(dec_r,    'D')),
        0x16: (f'LD D,n',      partial(ld_r_n,   'D')),
        0x17: (f'RLA',         rla),
        0x18: (f'JR e',        jr_c_e),
        0x19: (f'ADD {hl},DE', partial(add_dd_ss, hl,  'DE')),
        0x1A: (f'LD A,(DE)',   partial(ld_r_pdd,  'A', 'DE')),
        0x1B: (f'DEC DE',      partial(dec_ss,    'DE')),
        0x1C: (f'INC E',       partial(inc_r,     'E')),
        0x1D: (f'DEC E',       partial(dec_r,     'E')),
        0x1E: (f'LD E,n',      partial(ld_r_n,    'E')),
        0x1F: (f'RRA',         rra),

        # 2x: complete.
        0x20: (f'JR NZ,e',       partial(jr_c_e,    '!ZF')),
        0x21: (f'LD {hl},nn',    partial(ld_dd_nn,  hl)),
        0x22: (f'LD (nn),{hl}',  partial(ld_pnn_dd, hl)),
        0x23: (f'INC {hl}',      partial(inc_ss,    hl)),
        0x24: (f'INC {h}',       partial(inc_r,     h)),
        0x25: (f'DEC {h}',       partial(dec_r,     h)),
        0x26: (f'LD {h},n',      partial(ld_r_n,    h)),
        0x27: (f'DAA',           daa),
        0x28: (f'JR Z,e',        partial(jr_c_e,    'ZF')),
        0x29: (f'ADD {hl},{hl}', partial(add_dd_ss, hl, hl)),
        0x2A: (f'LD {hl},(nn)',  partial(ld_dd_pnn, hl)),
        0x2B: (f'DEC {hl}',      partial(dec_ss,    hl)),
        0x2C: (f'INC {l}',       partial(inc_r,     l)),
        0x2D: (f'DEC {l}',       partial(dec_r,     l)),
        0x2E: (f'LD {l},n',      partial(ld_r_n,    l)),
        0x2F: (f'CPL',           cpl),

        # 3x: complete.
        0x30: (f'JR NC,e',      partial(jr_c_e, '!CF')),
        0x31: (f'LD SP,nn',     partial(ld_dd_nn, 'SP')),
        0x32: (f'LD (nn),A',    ld_pnn_a),
        0x33: (f'INC SP',       partial(inc_ss, 'SP')),
        0x34: (f'INC ({hld})',  partial(inc_pdd, xy)),
        0x35: (f'DEC ({hld})',  partial(dec_pdd, xy)),
        0x36: (f'LD ({hld}),n', partial(ld_pss_n, xy)),
        0x37: (f'SCF',          scf),
        0x38: (f'JR C,e',       partial(jr_c_e, 'CF')),
        0x39: (f'ADD {hl},SP',  partial(add_dd_ss, hl, 'SP')),
        0x3A: (f'LD A,(nn)',    ld_A_pnn),
        0x3B: (f'DEC SP',       partial(dec_ss, 'SP')),
        0x3C: (f'INC A',        partial(inc_r, 'A')),
        0x3D: (f'DEC A',        partial(dec_r, 'A')),
        0x3E: (f'LD A,n',       partial(ld_r_n, 'A')),
        0x3F: (f'CCF',          ccf),

        # 4x: complete.
        0x40: (f'LD B,B',      partial(ld_r_r,   'B', 'B')),
        0x41: (f'LD B,C',      partial(ld_r_r,   'B', 'C')),
        0x42: (f'LD B,D',      partial(ld_r_r,   'B', 'D')),
        0x43: (f'LD B,E',      partial(ld_r_r,   'B', 'E')),
        0x44: (f'LD B,{h}',    partial(ld_r_r,   'B', h)),
        0x45: (f'LD B,{l}',    partial(ld_r_r,   'B', l)),
        0x46: (f'LD B,({hld})', partial(ld_r_pdd, 'B', 'HL', xy)),
        0x47: (f'LD B,A',       partial(ld_r_r,   'B', 'A')),
        0x48: (f'LD C,B',       partial(ld_r_r,   'C', 'B')),
        0x49: (f'LD C,C',       partial(ld_r_r,   'C', 'C')),
        0x4A: (f'LD C,D',       partial(ld_r_r,   'C', 'D')),
        0x4B: (f'LD C,E',       partial(ld_r_r,   'C', 'E')),
        0x4C: (f'LD C,{h}',     partial(ld_r_r,   'C', h)),
        0x4D: (f'LD C,{l}',     partial(ld_r_r,   'C', l)),
        0x4E: (f'LD C,({hld})', partial(ld_r_pdd, 'C', 'HL', xy)),
        0x4F: (f'LD C,A',       partial(ld_r_r,   'C', 'A')),

        # 5x: complete.
        0x50: (f'LD D,B',       partial(ld_r_r,   'D', 'B')),
        0x51: (f'LD D,C',       partial(ld_r_r,   'D', 'C')),
        0x52: (f'LD D,D',       partial(ld_r_r,   'D', 'D')),
        0x53: (f'LD D,E',       partial(ld_r_r,   'D', 'E')),
        0x54: (f'LD D,{h}',     partial(ld_r_r,   'D', h)),
        0x55: (f'LD D,{l}',     partial(ld_r_r,   'D', l)),
        0x56: (f'LD D,({hld})', partial(ld_r_pdd, 'D', 'HL', xy)),
        0x57: (f'LD D,A',       partial(ld_r_r,   'D', 'A')),
        0x58: (f'LD E,B',       partial(ld_r_r,   'E', 'B')),
        0x59: (f'LD E,C',       partial(ld_r_r,   'E', 'C')),
        0x5A: (f'LD E,D',       partial(ld_r_r,   'E', 'D')),
        0x5B: (f'LD E,E',       partial(ld_r_r,   'E', 'E')),
        0x5C: (f'LD E,{h}',     partial(ld_r_r,   'E', h)),
        0x5D: (f'LD E,{l}',     partial(ld_r_r,   'E', l)),
        0x5E: (f'LD E,({hld})', partial(ld_r_pdd, 'E', 'HL', xy)),
        0x5F: (f'LD E,A',       partial(ld_r_r,   'E', 'A')),

        # 6x: complete.
        0x60: (f'LD {h},B',                      partial(ld_r_r,   h, 'B')),
        0x61: (f'LD {h},C',                      partial(ld_r_r,   h, 'C')),
        0x62: (f'LD {h},D',                      partial(ld_r_r,   h, 'D')),
        0x63: (f'LD {h},E',                      partial(ld_r_r,   h, 'E')),
        0x64: (f'LD {h},{h}',                    partial(ld_r_r,   h, h)),
        0x65: (f'LD {h},{l}',                    partial(ld_r_r,   h, l)),
        0x66: (f'LD {"H" if xy else h},({hld})', partial(ld_r_pdd, 'H' if xy else h, 'HL', xy)),
        0x67: (f'LD {h},A',                      partial(ld_r_r,   h, 'A')),
        0x68: (f'LD {l},B',                      partial(ld_r_r,   l, 'B')),
        0x69: (f'LD {l},C',                      partial(ld_r_r,   l, 'C')),
        0x6A: (f'LD {l},D',                      partial(ld_r_r,   l, 'D')),
        0x6B: (f'LD {l},E',                      partial(ld_r_r,   l, 'E')),
        0x6C: (f'LD {l},{h}',                    partial(ld_r_r,   l, h)),
        0x6D: (f'LD {l},{l}',                    partial(ld_r_r,   l, l)),
        0x6E: (f'LD {"L" if xy else l},({hld})', partial(ld_r_pdd, 'L' if xy else l, 'HL', xy)),
        0x6F: (f'LD {l},A',                      partial(ld_r_r,   l, 'A')),

        # 7x: complete.
        0x70: (f'LD ({hld}),B',                  partial(ld_pdd_r, 'HL', 'B', xy)),
        0x71: (f'LD ({hld}),C',                  partial(ld_pdd_r, 'HL', 'C', xy)),
        0x72: (f'LD ({hld}),D',                  partial(ld_pdd_r, 'HL', 'D', xy)),
        0x73: (f'LD ({hld}),E',                  partial(ld_pdd_r, 'HL', 'E', xy)),
        0x74: (f'LD ({hld}),{"H" if xy else h}', partial(ld_pdd_r, 'HL', 'H' if xy else h, xy)),
        0x75: (f'LD ({hld}),{"L" if xy else l}', partial(ld_pdd_r, 'HL', 'L' if xy else l, xy)),
        0x76: (f'HALT',                          halt),
        0x77: (f'LD ({hld}),A',                  partial(ld_pdd_r, 'HL', 'A', xy)),
        0x78: (f'LD A,B',                        partial(ld_r_r,   'A',  'B')),
        0x79: (f'LD A,C',                        partial(ld_r_r,   'A',  'C')),
        0x7A: (f'LD A,D',                        partial(ld_r_r,   'A',  'D')),
        0x7B: (f'LD A,E',                        partial(ld_r_r,   'A',  'E')),
        0x7C: (f'LD A,{h}',                      partial(ld_r_r,   'A',  h)),
        0x7D: (f'LD A,{l}',                      partial(ld_r_r,   'A',  l)),
        0x7E: (f'LD A,({hld})',                  partial(ld_r_pdd, 'A',  'HL', xy)),
        0x7F: (f'LD A,A',                        partial(ld_r_r,   'A',  'A')),

        # 8x: complete.
        0x80: (f'ADD A,B',       partial(add_A_r, 'B')),
        0x81: (f'ADD A,C',       partial(add_A_r, 'C')),
        0x82: (f'ADD A,D',       partial(add_A_r, 'D')),
        0x83: (f'ADD A,E',       partial(add_A_r, 'E')),
        0x84: (f'ADD A,{h}',     partial(add_A_r, h)),
        0x85: (f'ADD A,{l}',     partial(add_A_r, l)),
        0x86: (f'ADD A,({hld})', partial(add_A_pss, xy)),
        0x87: (f'ADD A,A',       partial(add_A_r, 'A')),
        0x88: (f'ADC A,B',       partial(adc_A_r, 'B')),
        0x89: (f'ADC A,C',       partial(adc_A_r, 'C')),
        0x8A: (f'ADC A,D',       partial(adc_A_r, 'D')),
        0x8B: (f'ADC A,E',       partial(adc_A_r, 'E')),
        0x8C: (f'ADC A,{h}',     partial(adc_A_r, h)),
        0x8D: (f'ADC A,{l}',     partial(adc_A_r, l)),
        0x8E: (f'ADC A,({hld})', partial(adc_A_pss, xy)),
        0x8F: (f'ADC A,A',       partial(adc_A_r, 'A')),

        # 9x: complete.
        0x90: (f'SUB B',         partial(sub_r,   'B')),
        0x91: (f'SUB C',         partial(sub_r,   'C')),
        0x92: (f'SUB D',         partial(sub_r,   'D')),
        0x93: (f'SUB E',         partial(sub_r,   'E')),
        0x94: (f'SUB {h}',       partial(sub_r,   h)),
        0x95: (f'SUB {l}',       partial(sub_r,   l)),
        0x96: (f'SUB ({hld})',   partial(sub_pss, xy)),
        0x97: (f'SUB A',         partial(sub_r,   'A')),
        0x98: (f'SBC A,B',       partial(sbc_A_r, 'B')),
        0x99: (f'SBC A,C',       partial(sbc_A_r, 'C')),
        0x9A: (f'SBC A,D',       partial(sbc_A_r, 'D')),
        0x9B: (f'SBC A,E',       partial(sbc_A_r, 'E')),
        0x9C: (f'SBC A,{h}',     partial(sbc_A_r, h)),
        0x9D: (f'SBC A,{l}',     partial(sbc_A_r, l)),
        0x9E: (f'SBC A,({hld})', partial(sbc_A_pss, xy)),
        0x9F: (f'SBC A,A',       partial(sbc_A_r, 'A')),

        # Ax: complete.
        0xA0: (f'AND B',       partial(logical_r,   '&', 'B')),
        0xA1: (f'AND C',       partial(logical_r,   '&', 'C')),
        0xA2: (f'AND D',       partial(logical_r,   '&', 'D')),
        0xA3: (f'AND E',       partial(logical_r,   '&', 'E')),
        0xA4: (f'AND {h}',     partial(logical_r,   '&', h)),
        0xA5: (f'AND {l}',     partial(logical_r,   '&', l)),
        0xA6: (f'AND ({hld})', partial(logical_pss, '&', xy)),
        0xA7: (f'AND A',       partial(logical_r,   '&', 'A')),
        0xA8: (f'XOR B',       partial(logical_r,   '^', 'B')),
        0xA9: (f'XOR C',       partial(logical_r,   '^', 'C')),
        0xAA: (f'XOR D',       partial(logical_r,   '^', 'D')),
        0xAB: (f'XOR E',       partial(logical_r,   '^', 'E')),
        0xAC: (f'XOR {h}',     partial(logical_r,   '^', h)),
        0xAD: (f'XOR {l}',     partial(logical_r,   '^', l)),
        0xAE: (f'XOR ({hld})', partial(logical_pss, '^', xy)),
        0xAF: (f'XOR A',       partial(logical_r,   '^', 'A')),

        # Bx: complete.
        0xB0: (f'OR B',       partial(logical_r,   '|', 'B')),
        0xB1: (f'OR C',       partial(logical_r,   '|', 'C')),
        0xB2: (f'OR D',       partial(logical_r,   '|', 'D')),
        0xB3: (f'OR E',       partial(logical_r,   '|', 'E')),
        0xB4: (f'OR {h}',     partial(logical_r,   '|', h)),
        0xB5: (f'OR {l}',     partial(logical_r,   '|', l)),
        0xB6: (f'OR ({hld})', partial(logical_pss, '|', xy)),
        0xB7: (f'OR A',       partial(logical_r,   '|', 'A')),
        0xB8: (f'CP B',       partial(cp_r, 'B')),
        0xB9: (f'CP C',       partial(cp_r, 'C')),
        0xBA: (f'CP D',       partial(cp_r, 'D')),
        0xBB: (f'CP E',       partial(cp_r, 'E')),
        0xBC: (f'CP {h}',     partial(cp_r, h)),
        0xBD: (f'CP {l}',     partial(cp_r, l)),
        0xBE: (f'CP ({hld})', partial(cp_pss, xy)),
        0xBF: (f'CP A',       partial(cp_r, 'A')),

        # Cx: complete.
        0xC0: (f'RET NZ',     partial(ret,  '!(F & ZF_MASK)')),
        0xC1: (f'POP BC',     partial(pop_qq, 'BC')),
        0xC2: (f'JP NZ,nn',   partial(jp,   '!(F & ZF_MASK)')),
        0xC3: (f'JP nn',      jp),
        0xC4: (f'CALL NZ,nn', partial(call, '!(F & ZF_MASK)')),
        0xC5: (f'PUSH BC',    partial(push_qq, 'BC')),
        0xC6: (f'ADD A,n',    add_A_n),
        0xC7: (f'RST $00',    partial(rst, 0x00)),
        0xC8: (f'RET Z',      partial(ret,  'F & ZF_MASK')),
        0xC9: (f'RET',        ret),
        0xCA: (f'JP Z,nn',    partial(jp,   'F & ZF_MASK')),
        0xCB: cb_table(xy),
        0xCC: (f'CALL Z,nn',  partial(call, 'F & ZF_MASK')),
        0xCD: (f'CALL nn',    call),
        0xCE: (f'ADC A,n',    adc_A_n),
        0xCF: (f'RST $08',    partial(rst, 0x08)),

        # Dx: complete.
        0xD0: (f'RET NC',     partial(ret,   '!(F & CF_MASK)')),
        0xD1: (f'POP DE',     partial(pop_qq, 'DE')),
        0xD2: (f'JP NC,nn',   partial(jp,   '!(F & CF_MASK)')),
        0xD3: (f'OUT (n),A',  out_n_a),
        0xD4: (f'CALL NC,nn', partial(call, '!(F & CF_MASK)')),
        0xD5: (f'PUSH DE',    partial(push_qq, 'DE')),
        0xD6: (f'SUB n',      sub_n),
        0xD7: (f'RST $10',    partial(rst, 0x10)),
        0xD8: (f'RET C',      partial(ret,  'F & CF_MASK')),
        0xD9: (f'EXX',        exx),
        0xDA: (f'JP C,nn',    partial(jp,   'F & CF_MASK')),
        0xDB: (f'IN A,(n)',   in_A_n),
        0xDC: (f'CALL C,nn',  partial(call, 'F & CF_MASK')),
        0xDD: None,
        0xDE: (f'SBC A,n',    sbc_A_n),
        0xDF: (f'RST $18',    partial(rst, 0x18)),

        # Ex: complete.
        0xE0: (f'RET PO',       partial(ret,  '!(F & PF_MASK)')),
        0xE1: (f'POP {hl}',     partial(pop_qq, hl)),
        0xE2: (f'JP PO,nn',     partial(jp,   '!(F & PF_MASK)')),
        0xE3: (f'EX (SP),{hl}', partial(ex_pSP_dd, hl)),
        0xE4: (f'CALL PO,nn',   partial(call, '!(F & PF_MASK)')),
        0xE5: (f'PUSH {hl}',    partial(push_qq, hl)),
        0xE6: (f'AND n',        partial(logical_n, '&')),
        0xE7: (f'RST $20',      partial(rst, 0x20)),
        0xE8: (f'RET PE',       partial(ret,  'F & PF_MASK')),
        0xE9: (f'JP ({hl})',    partial(jp_pss, hl)),
        0xEA: (f'JP PE,nn',     partial(jp,   'F & PF_MASK')),
        0xEB: (f'EX DE,HL',     partial(ex, 'DE', 'HL')),
        0xEC: (f'CALL PE,nn',   partial(call, 'F & PF_MASK')),
        0xED: None,
        0xEE: (f'XOR n',        partial(logical_n, '^')),
        0xEF: (f'RST $28',      partial(rst, 0x28)),

        # Fx: complete.
        0xF0: (f'RET P',       partial(ret,   '!(F & SF_MASK)')),
        0xF1: (f'POP AF',      partial(pop_qq, 'AF')),
        0xF2: (f'JP P,nn',     partial(jp,   '!(F & SF_MASK)')),
        0xF3: (f'DI',          di),
        0xF4: (f'CALL P,nn',   partial(call, '!(F & SF_MASK)')),
        0xF5: (f'PUSH AF',     partial(push_qq, 'AF')),
        0xF6: (f'OR n',        partial(logical_n, '|')),
        0xF7: (f'RST $30',     partial(rst, 0x30)),
        0xF8: (f'RET M',       partial(ret,  'F & SF_MASK')),
        0xF9: (f'LD SP,{hl}',  partial(ld_dd_ss, 'SP', hl)),
        0xFA: (f'JP M,nn',     partial(jp,   'F & SF_MASK')),
        0xFB: (f'EI',          ei),
        0xFC: (f'CALL M,nn',   partial(call, 'F & SF_MASK')),
        0xFD: None,
        0xFE: (f'CP n',        cp_n),
        0xFF: (f'RST $38',     partial(rst, 0x38)),
    }

    if not xy:
        t.update({
            0xED: ed_table(),
            0xDD: table('IX'),
            0xFD: table('IY'),
        })

    return t


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

    statement = 'log_dbg("'
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
    s = 'log_dbg("'
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

    # Show on the registers before and after each instruction execution,
    # as well as a disassembly of each executed instruction.
    debug = False

    if debug:
        if not prefix:
            f.write('''
log_dbg("     AF %04X BC %04X DE %04X HL %04X IX %04X IY %04X F %s%s-%s-%s%s%s\\n", AF, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
log_dbg("     AF'%04X BC'%04X DE'%04X HL'%04X PC %04X SP %04X I %02X\\n", AF_, BC_, DE_, HL_, PC, SP, I);
log_dbg("     ROM %d  DIVMMC %02X  PAGES %02X %02X %02X %02X %02X %02X %02X %02X\\n", 0xFF, divmmc_control_read(0xE3), mmu_page_get(0), mmu_page_get(1), mmu_page_get(2), mmu_page_get(3), mmu_page_get(4), mmu_page_get(5), mmu_page_get(6), mmu_page_get(7));
log_dbg("%04X ", PC);
''')

    if prefix == [0xDD, 0xCB] or prefix == [0xFD, 0xCB]:
        # Special opcode where 3rd byte is parameter and 4th byte needed for
        # decoding. Read 4th byte, but keep PC at 3rd byte.
        read_opcode    = 'opcode = memory_read(PC + 1)'
        post_increment = 'PC++;'
    else:
        read_opcode    = 'opcode = memory_read(PC++)'
        post_increment = ''

    f.write(f'''
{read_opcode};
T(4);
''')
    if not prefix:
        f.write('R = (R & 0x80) | (++R & 0x7F);\n')
    f.write('switch (opcode) {\n')

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
  {post_increment}
  break;

''')

        elif isinstance(item, dict):
            f.write(f'case 0x{opcode:02X}:\n')
            generate(item, f, prefix + [opcode])

    optional_break = 'break;' if prefix else ''
    f.write(f'''
default:
  log_dbg("cpu: unknown opcode {prefix_str}$%02X at $%04X\\n", opcode, PC - 1 - {prefix_len});
  return -1;
}}
{optional_break}
''')


def generate_fast(instructions: Table, prefix: List[Opcode], functions: Dict[str, C], tables: Dict[str, C]) -> C:
    table = {}

    for opcode, entry in instructions.items():
        name = 'opcode_' + '_'.join(f'{op:02X}' for op in prefix + [opcode])

        if isinstance(entry, tuple):
            comment, implementation = entry
            body                     = implementation()

        elif isinstance(entry, dict):
            comment = 'prefix'
            body    = generate_fast(entry, prefix + [opcode], functions, tables)

        functions[name] = (comment, body)
        table[opcode]   = name

    missing = set(range(256)) - set(table.keys())
    for opcode in missing:
        name = 'opcode_' + '_'.join(f'{op:02X}' for op in prefix + [opcode])
        body = f'''
            log_err("cpu: {name} not implemented\\n");
            exit(1);
        '''

        functions[name] = (comment, body)
        table[opcode]   = name

    # Write the lookup table.
    name         = 'lookup_' + '_'.join(f'{op:02X}' for op in prefix) if prefix else 'lookup'
    body         = ',\n'.join(table[op] for op in sorted(table))
    tables[name] = body

    if prefix == [0xDD, 0xCB] or prefix == [0xFD, 0xCB]:
        # Special opcode where 3rd byte is parameter and 4th byte needed for
        # decoding. Read 4th byte, but keep PC at 3rd byte.
        return f'''
const u8_t opcode = memory_read(PC + 1); T(4);
{name}[opcode]();
PC++;
'''

    # Return the body that uses the table.
    return f'''
const u8_t opcode = memory_read(PC++); T(4);
{name}[opcode]();
'''


def main_fast() -> None:
    functions    = {}
    tables       = {}
    instructions = table()
    decoder      = generate_fast(instructions, [], functions, tables)

    with open('opcodes.c', 'w') as f:
        f.write('typedef void (*opcode_impl_t)(void);\n')

        for name in sorted(functions):
            f.write(f'static void {name}(void);\n')

        f.write('\n');
        for name in sorted(tables):
            body = tables[name]
            f.write(f'''
static opcode_impl_t {name}[256] = {{
{body}
}};
''')

        f.write('\n');
        for name in sorted(functions):
            (comment, body) = functions[name];
            f.write(f'''
/* {comment} */
static void {name}(void) {{
{body}
}}
''')

        f.write(f'''
void cpu_execute_next_opcode(void) {{
  {decoder}
}}
''')


def main() -> None:
    with open('opcodes.c', 'w') as f:
        generate(table(), f)


if __name__ == '__main__':
    main_fast()
