opcode = memory_read(PC++);
clock_ticks(4);
switch (opcode) {
  case 0x01:  /* LD BC,nn */
    fprintf(stderr, "%04Xh LD BC,nn             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    C = memory_read(PC++);
    clock_ticks(3);
    B = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x03:  /* INC BC */
    fprintf(stderr, "%04Xh INC BC               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    BC++;
    clock_ticks(2);
    break;

  case 0x04:  /* INC B */
    fprintf(stderr, "%04Xh INC B                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF = HALFCARRY(B, 1, B + 1);
    B++;
    SF = SIGN(B);
    ZF = B == 0;
    VF = B == 0x80;
    NF = 0;
    break;

  case 0x06:  /* LD B,n */
    fprintf(stderr, "%04Xh LD B,n               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    B = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x08:  /* EX AF,AF' */
    fprintf(stderr, "%04Xh EX AF,AF'            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    cpu_flags_pack();
    cpu_exchange(&AF, &AF_);
    cpu_flags_unpack();
    break;

  case 0x0A:  /* DEC BC */
    fprintf(stderr, "%04Xh DEC BC               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    BC--;
    clock_ticks(2);
    break;

  case 0x0C:  /* INC C */
    fprintf(stderr, "%04Xh INC C                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF = HALFCARRY(C, 1, C + 1);
    C++;
    SF = SIGN(C);
    ZF = C == 0;
    VF = C == 0x80;
    NF = 0;
    break;

  case 0x10:  /* DJNZ e */
    fprintf(stderr, "%04Xh DJNZ e               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    clock_ticks(1);
    Z = memory_read(PC++);
    clock_ticks(3);
    if (--B) {
      PC += (s8_t) Z;
    clock_ticks(5);
    }
    break;

  case 0x11:  /* LD DE,nn */
    fprintf(stderr, "%04Xh LD DE,nn             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    E = memory_read(PC++);
    clock_ticks(3);
    D = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x16:  /* LD D,n */
    fprintf(stderr, "%04Xh LD D,n               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    D = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x18:  /* JR e */
    fprintf(stderr, "%04Xh JR e                 A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    PC += (s8_t) Z;
    clock_ticks(5);
    break;

  case 0x1D:  /* DEC E */
    fprintf(stderr, "%04Xh DEC E                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF = HALFCARRY(E, -1, E - 1);
    VF = E == 0x80;
    E--;
    SF = SIGN(E);
    ZF = E == 0;
    NF = 1;
    break;

  case 0x20:  /* JR NZ,e */
    fprintf(stderr, "%04Xh JR NZ,e              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    if (!ZF) {
      PC += (s8_t) Z;
    clock_ticks(5);
    }
    break;

  case 0x21:  /* LD HL,nn */
    fprintf(stderr, "%04Xh LD HL,nn             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    L = memory_read(PC++);
    clock_ticks(3);
    H = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x23:  /* INC HL */
    fprintf(stderr, "%04Xh INC HL               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HL++;
    clock_ticks(2);
    break;

  case 0x28:  /* JR Z,e */
    fprintf(stderr, "%04Xh JR Z,e               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    if (ZF) {
      PC += (s8_t) Z;
    clock_ticks(5);
    }
    break;

  case 0x2B:  /* DEC HL */
    fprintf(stderr, "%04Xh DEC HL               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HL--;
    clock_ticks(2);
    break;

  case 0x2F:  /* CPL */
    fprintf(stderr, "%04Xh CPL                  A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = ~A;
    HF = NF = 1;
    break;

  case 0x30:  /* JR NC,e */
    fprintf(stderr, "%04Xh JR NC,e              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    if (!CF) {
      PC += (s8_t) Z;
    clock_ticks(5);
    }
    break;

  case 0x31:  /* LD SP,nn */
    fprintf(stderr, "%04Xh LD SP,nn             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    P = memory_read(PC++);
    clock_ticks(3);
    S = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x32:  /* LD nn,A */
    fprintf(stderr, "%04Xh LD nn,A              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    W = memory_read(PC++);
    clock_ticks(3);
    memory_write(WZ, A);
    clock_ticks(3);
    break;

  case 0x36:  /* LD (HL),n */
    fprintf(stderr, "%04Xh LD (HL),n            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    memory_write(HL, Z);
    clock_ticks(3);
    break;

  case 0x38:  /* JR C,e */
    fprintf(stderr, "%04Xh JR C,e               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(PC++);
    clock_ticks(3);
    if (C) {
      PC += (s8_t) Z;
    clock_ticks(5);
    }
    break;

  case 0x3C:  /* INC A */
    fprintf(stderr, "%04Xh INC A                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF = HALFCARRY(A, 1, A + 1);
    A++;
    SF = SIGN(A);
    ZF = A == 0;
    VF = A == 0x80;
    NF = 0;
    break;

  case 0x3D:  /* DEC A */
    fprintf(stderr, "%04Xh DEC A                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF = HALFCARRY(A, -1, A - 1);
    VF = A == 0x80;
    A--;
    SF = SIGN(A);
    ZF = A == 0;
    NF = 1;
    break;

  case 0x3E:  /* LD A,n */
    fprintf(stderr, "%04Xh LD A,n               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x46:  /* LD B,(HL) */
    fprintf(stderr, "%04Xh LD B,(HL)            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    B = memory_read(HL);
    clock_ticks(3);
    break;

  case 0x4B:  /* LD C,E */
    fprintf(stderr, "%04Xh LD C,E               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    C = B;
    break;

  case 0x4E:  /* LD C,(HL) */
    fprintf(stderr, "%04Xh LD C,(HL)            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    C = memory_read(HL);
    clock_ticks(3);
    break;

  case 0x6F:  /* LD L,A */
    fprintf(stderr, "%04Xh LD L,A               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    L = A;
    break;

  case 0x75:  /* LD (HL),L */
    fprintf(stderr, "%04Xh LD (HL),L            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    memory_write(HL, L);
    clock_ticks(3);
    break;

  case 0x77:  /* LD (HL),A */
    fprintf(stderr, "%04Xh LD (HL),A            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    memory_write(HL, A);
    clock_ticks(3);
    break;

  case 0x79:  /* LD A,C */
    fprintf(stderr, "%04Xh LD A,C               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = C;
    break;

  case 0x7A:  /* LD A,D */
    fprintf(stderr, "%04Xh LD A,D               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = D;
    break;

  case 0x7C:  /* LD A,H */
    fprintf(stderr, "%04Xh LD A,H               A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = H;
    break;

  case 0x7E:  /* LD A,(HL) */
    fprintf(stderr, "%04Xh LD A,(HL)            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = memory_read(HL);
    clock_ticks(3);
    break;

  case 0x80:  /* ADD A,B */
    fprintf(stderr, "%04Xh ADD A,B              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF  = HALFCARRY(A, B, A + B);
    VF  = SIGN(A) == SIGN(B);
    A  += B;
    VF &= SIGN(B) != SIGN(A);
    SF  = SIGN(A);
    ZF  = A == 0;
    NF  = 0;
    CF  = ((u16_t) A) + B > 255;
    break;

  case 0x87:  /* ADD A,A */
    fprintf(stderr, "%04Xh ADD A,A              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    HF  = HALFCARRY(A, A, A + A);
    VF  = SIGN(A) == SIGN(A);
    A  += A;
    VF &= SIGN(A) != SIGN(A);
    SF  = SIGN(A);
    ZF  = A == 0;
    NF  = 0;
    CF  = ((u16_t) A) + A > 255;
    break;

  case 0xA2:  /* AND D */
    fprintf(stderr, "%04Xh AND D                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A &= D;
    SF = SIGN(A);
    ZF = A == 0;
    HF = 1;
    PF = parity[A];
    NF = CF = 0;
    break;

  case 0xA7:  /* AND A */
    fprintf(stderr, "%04Xh AND A                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A &= A;
    SF = SIGN(A);
    ZF = A == 0;
    HF = 1;
    PF = parity[A];
    NF = CF = 0;
    break;

  case 0xAF:  /* XOR A */
    fprintf(stderr, "%04Xh XOR A                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A = 0;
    SF = HF = NF = CF = 0;
    ZF = PF = 1;
    break;

  case 0xC3:  /* JP nn */
    fprintf(stderr, "%04Xh JP nn                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z  = memory_read(PC);
    clock_ticks(3);
    W  = memory_read(PC + 1);
    clock_ticks(3);
    PC = WZ;
    break;

  case 0xC5:  /* PUSH BC */
    fprintf(stderr, "%04Xh PUSH BC              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    clock_ticks(1);
    memory_write(--SP, B);
    clock_ticks(3);
    memory_write(--SP, C);
    clock_ticks(3);
    break;

  case 0xC9:  /* RET */
    fprintf(stderr, "%04Xh RET                  A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    PCL = memory_read(SP++);
    clock_ticks(3);
    PCH = memory_read(SP++);
    clock_ticks(3);
    break;

  case 0xCB:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x02:  /* RLC D */
        fprintf(stderr, "%04Xh RLC D                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        CF  = D >> 7;
        D = D << 1 | CF;
        SF = SIGN(D);
        ZF = D == 0;
        HF = 0;
        PF = parity[D];
        NF = 0;
        break;

      case 0x3F:  /* SRL A */
        fprintf(stderr, "%04Xh SRL A                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        CF = A & 1;
        A >>= 1;
        SF = HF = NF = 0;
        ZF = A == 0;
        PF = parity[A];
        break;

      default:
        fprintf(stderr, "Unknown opcode CBh %02Xh at %04Xh\n", opcode, PC - 2);
        return -1;
    }
    break;

  case 0xD9:  /* EXX */
    fprintf(stderr, "%04Xh EXX                  A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    cpu_exchange(&BC, &BC_);
    cpu_exchange(&DE, &DE_);
    cpu_exchange(&HL, &HL_);
    break;

  case 0xDD:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x6F:  /* LD IXL,A */
        fprintf(stderr, "%04Xh LD IXL,A             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        IXL = A;
        break;

      case 0x7D:  /* LD A,IXL */
        fprintf(stderr, "%04Xh LD A,IXL             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        A = IXL;
        break;

      default:
        fprintf(stderr, "Unknown opcode DDh %02Xh at %04Xh\n", opcode, PC - 2);
        return -1;
    }
    break;

  case 0xE3:  /* EX (SP),HL */
    fprintf(stderr, "%04Xh EX (SP),HL           A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z = memory_read(SP);
    clock_ticks(3);
    memory_write(SP,     L);
    clock_ticks(4);
    W = memory_read(SP + 1);
    clock_ticks(3);
    memory_write(SP + 1, H);
    clock_ticks(5);
    H = W;
    L = Z;
    break;

  case 0xE6:  /* AND n */
    fprintf(stderr, "%04Xh AND n                A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    A &= memory_read(PC++);
    SF = SIGN(A);
    ZF = A == 0;
    HF = 1;
    PF = parity[A];
    NF = CF = 0;
    clock_ticks(3);
    break;

  case 0xE7:  /* RST 20h */
    fprintf(stderr, "%04Xh RST 20h              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    clock_ticks(1);
    memory_write(--SP, PCH);
    clock_ticks(3);
    memory_write(--SP, PCL);
    PC = 0x20;
    clock_ticks(3);
    break;

  case 0xED:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x43:  /* LD (nn),BC */
        fprintf(stderr, "%04Xh LD (nn),BC           A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        Z = memory_read(PC++);
        clock_ticks(3);
        W = memory_read(PC++);
        clock_ticks(3);
        memory_write(WZ,     C);
        clock_ticks(3);
        memory_write(WZ + 1, B);
        clock_ticks(3);
        break;

      case 0x4B:  /* LD BC,(nn) */
        fprintf(stderr, "%04Xh LD BC,(nn)           A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        C = memory_read(PC++);
        clock_ticks(3);
        B = memory_read(PC++);
        clock_ticks(3);
        break;

      case 0x51:  /* OUT (C),D */
        fprintf(stderr, "%04Xh OUT (C),D            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        io_write(BC, D);
        clock_ticks(4);
        break;

      case 0x52:  /* SBC HL,DE */
        fprintf(stderr, "%04Xh SBC HL,DE            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        WZ  = HL;
        HL -= DE + CF;
        clock_ticks(4);
        SF  = SIGN(H);
        ZF  = HL == 0;
        HF  = HALFCARRY16(WZ, -(DE + CF), HL);
        VF  = SIGN(W) == SIGN(D) && SIGN(D) != SIGN(H);
        NF  = 1;
        CF  = HL < DE;
        clock_ticks(3);
        break;

      case 0x78:  /* IN A,(C) */
        fprintf(stderr, "%04Xh IN A,(C)             A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        A  = io_read(BC);
        SF = SIGN(A);
        ZF = A == 0;
        HF = NF = 0;
        PF = parity[A];
        clock_ticks(4);
        break;

      case 0x79:  /* OUT (C),A */
        fprintf(stderr, "%04Xh OUT (C),A            A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        io_write(BC, A);
        clock_ticks(4);
        break;

      case 0x8A:  /* PUSH nn */
        fprintf(stderr, "%04Xh PUSH nn              A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        Z = memory_read(PC++);
        W = memory_read(PC++);
        memory_write(--SP, W);
        memory_write(--SP, Z);
        clock_ticks(11);
        break;

      case 0x91:  /* NEXTREG reg,value */
        fprintf(stderr, "%04Xh NEXTREG reg,value    A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, memory_read(PC++));
        clock_ticks(8);
        break;

      case 0x92:  /* NEXTREG reg,A */
        fprintf(stderr, "%04Xh NEXTREG reg,A        A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, A);
        clock_ticks(4);
        break;

      case 0xB0:  /* LDIR */
        fprintf(stderr, "%04Xh LDIR                 A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        Z  = memory_read(HL++);
        clock_ticks(3);
        memory_write(DE++, Z);
        HF = NF = 0;
        VF = BC - 1 != 0;
        clock_ticks(5);
        if (--BC) {
          PC -= 2;
        clock_ticks(5);
        }
        break;

      case 0xB8:  /* LDDR */
        fprintf(stderr, "%04Xh LDDR                 A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 1, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
        Z = memory_read(HL--);
        clock_ticks(3);
        memory_write(DE--, Z);
        HF = NF = 0;
        VF = BC - 1 != 0;
        clock_ticks(5);
        if (--BC) {
          PC -= 2;
        clock_ticks(5);
        }
        break;

      default:
        fprintf(stderr, "Unknown opcode EDh %02Xh at %04Xh\n", opcode, PC - 2);
        return -1;
    }
    break;

  case 0xF3:  /* DI */
    fprintf(stderr, "%04Xh DI                   A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    IFF1 = IFF2 = 0;
    break;

  case 0xFE:  /* CP n */
    fprintf(stderr, "%04Xh CP n                 A=%02Xh BC=%04Xh DE=%04Xh HL=%04Xh IX=%04Xh IY=%04Xh F=%s%s-%s-%s%s%s\n", PC - 1 - 0, A, BC, DE, HL, IX, IY, SF ? "S" : "s", ZF ? "Z" : "z", HF ? "H" : "h", PF ? "P/V" : "p/v", NF ? "N" : "n", CF ? "C" : "c");
    Z  = memory_read(PC++);
    SF = SIGN(A - Z);
    ZF = A - Z == 0;
    HF = HALFCARRY(A, Z, A - Z);
    VF = SIGN(A) == SIGN(Z) && SIGN(Z) != SIGN(A - Z);
    NF = 1;
    CF = A < Z;
    clock_ticks(3);
    break;

  default:
    fprintf(stderr, "Unknown opcode %02Xh at %04Xh\n", opcode, PC - 1);
    return -1;
}
