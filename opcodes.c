printf("%04Xh\n", PC);
opcode = memory_read(PC++);
clock_ticks(4);
switch (opcode) {
  case 0x01:  /* LD BC,nn */
    C = memory_read(PC++);
    clock_ticks(3);
    B = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x03:  /* INC BC */
    BC++;
    clock_ticks(2);
    break;

  case 0x04:  /* INC B */
    FH = HALFCARRY(B, 1, B + 1);
    B++;
    FS = SIGN(B);
    FZ = B == 0;
    FV = B = 0x80;
    FN = 0;
    break;

  case 0x08:  /* EX AF,AF' */
    cpu_flags_pack();
    cpu_exchange(&AF, &AF_);
    cpu_flags_unpack();
    break;

  case 0x0A:  /* DEC BC */
    BC--;
    clock_ticks(2);
    break;

  case 0x0C:  /* INC C */
    FH = HALFCARRY(C, 1, C + 1);
    C++;
    FS = SIGN(C);
    FZ = C == 0;
    FV = C = 0x80;
    FN = 0;
    break;

  case 0x10:  /* DJNZ e */
    clock_ticks(1);
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    if (--B) {
      PC += (s8_t) tmp8;
    clock_ticks(5);
    }
    break;

  case 0x11:  /* LD DE,nn */
    E = memory_read(PC++);
    clock_ticks(3);
    D = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x16:  /* LD D,n */
    D = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x18:  /* JR e */
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    PC += (s8_t) tmp8;
    clock_ticks(5);
    break;

  case 0x1D:  /* DEC E */
    FH = HALFCARRY(E, -1, E - 1);
    FV = E == 0x80;
    E--;
    FS = SIGN(E);
    FZ = E == 0;
    FN = 1;
    break;

  case 0x20:  /* JR NZ,e */
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    if (!FZ) {
      PC += (s8_t) tmp8;
    clock_ticks(5);
    }
    break;

  case 0x21:  /* LD HL,nn */
    L = memory_read(PC++);
    clock_ticks(3);
    H = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x23:  /* INC HL */
    HL++;
    clock_ticks(2);
    break;

  case 0x28:  /* JR Z,e */
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    if (FZ) {
      PC += (s8_t) tmp8;
    clock_ticks(5);
    }
    break;

  case 0x2B:  /* DEC HL */
    HL--;
    clock_ticks(2);
    break;

  case 0x2F:  /* CPL */
    A = ~A;
    FH = FN = 1;
    break;

  case 0x30:  /* JR NC,e */
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    if (!FC) {
      PC += (s8_t) tmp8;
    clock_ticks(5);
    }
    break;

  case 0x36:  /* LD (HL),n */
    tmp8 = memory_read(PC++);
    clock_ticks(3);
    memory_write(HL, tmp8);
    clock_ticks(3);
    break;

  case 0x3C:  /* INC A */
    FH = HALFCARRY(A, 1, A + 1);
    A++;
    FS = SIGN(A);
    FZ = A == 0;
    FV = A = 0x80;
    FN = 0;
    break;

  case 0x3E:  /* LD A,n */
    A = memory_read(PC++);
    clock_ticks(3);
    break;

  case 0x6F:  /* LD L,A */
    L = A;
    break;

  case 0x75:  /* LD (HL),L */
    memory_write(HL, L);
    clock_ticks(3);
    break;

  case 0x77:  /* LD (HL),A */
    memory_write(HL, A);
    clock_ticks(3);
    break;

  case 0x79:  /* LD A,C */
    A = C;
    break;

  case 0x7A:  /* LD A,D */
    A = D;
    break;

  case 0x7E:  /* LD A,(HL) */
    A = memory_read(HL);
    clock_ticks(3);
    break;

  case 0x87:  /* ADD A,A */
    FH  = HALFCARRY(A, A, A + A);
    FV  = SIGN(A) == SIGN(A);
    A  += A;
    FV &= SIGN(A) != SIGN(A);
    FS  = SIGN(A);
    FZ  = A == 0;
    FN  = 0;
    FC  = ((u16_t) A) + A > 255;
    break;

  case 0xA2:  /* AND D */
    A &= D;
    FS = SIGN(A);
    FZ = A == 0;
    FH = 1;
    FP = parity[A];
    FN = FC = 0;
    break;

  case 0xAF:  /* XOR A */
    A = 0;
    FS = FH = FN = FC = 0;
    FZ = FP = 1;
    break;

  case 0xC3:  /* JP nn */
    Z  = memory_read(PC);
    clock_ticks(3);
    W  = memory_read(PC + 1);
    clock_ticks(3);
    PC = WZ;
    break;

  case 0xCB:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x02:  /* RLC D */
        FC  = D >> 7;
        D = D << 1 | FC;
        FS = SIGN(D);
        FZ = D == 0;
        FH = 0;
        FP = parity[D];
        FN = 0;
        break;

      case 0x3F:  /* SRL A */
        FC = A & 1;
        A >>= 1;
        FS = FH = FN = 0;
        FZ = A == 0;
        FP = parity[A];
        break;

      default:
        fprintf(stderr, "Unknown opcode CBh %02Xh at %04Xh\n", opcode, cpu.pc - 2);
        return -1;
    }
    break;

  case 0xD9:  /* EXX */
    cpu_exchange(&BC, &BC_);
    cpu_exchange(&DE, &DE_);
    cpu_exchange(&HL, &HL_);
    break;

  case 0xDD:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x6F:  /* LD IXL,A */
        IXL = A;
        break;

      case 0x7D:  /* LD A,IXL */
        A = IXL;
        break;

      default:
        fprintf(stderr, "Unknown opcode DDh %02Xh at %04Xh\n", opcode, cpu.pc - 2);
        return -1;
    }
    break;

  case 0xE6:  /* AND n */
    A &= memory_read(PC++);
    FS = SIGN(A);
    FZ = A == 0;
    FH = 1;
    FP = parity[A];
    FN = FC = 0;
    clock_ticks(3);
    break;

  case 0xED:
    opcode = memory_read(PC++);
    clock_ticks(4);
    switch (opcode) {
      case 0x51:  /* OUT (C),D */
        io_write(BC, D);
        clock_ticks(4);
        break;

      case 0x78:  /* IN A,(C) */
        A = io_read(BC);
        FS = SIGN(A);
        FZ = A == 0;
        FH = FN = 0;
        FP = parity[A];
        clock_ticks(4);
        break;

      case 0x79:  /* OUT (C),A */
        io_write(BC, A);
        clock_ticks(4);
        break;

      case 0x91:  /* NEXTREG reg,value */
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, memory_read(PC++));
        clock_ticks(8);
        break;

      case 0x92:  /* NEXTREG reg,A */
        io_write(0x243B, memory_read(PC++));
        io_write(0x253B, A);
        clock_ticks(4);
        break;

      case 0xB0:  /* LDIR */
        tmp8 = memory_read(HL++);
        clock_ticks(3);
        memory_write(DE++, tmp8);
        FH = FN = 0;
        FV = BC - 1 != 0;
        clock_ticks(5);
        if (--BC) {
          PC -= 2;
        clock_ticks(5);
        }
        break;

      default:
        fprintf(stderr, "Unknown opcode EDh %02Xh at %04Xh\n", opcode, cpu.pc - 2);
        return -1;
    }
    break;

  case 0xF3:  /* DI */
    IFF1 = IFF2 = 0;
    break;

  case 0xFE:  /* CP n */
    tmp8 = memory_read(PC++);
    FS   = SIGN(A - tmp8);
    FZ   = A - tmp8 == 0;
    FH   = HALFCARRY(A, tmp8, A - tmp8);
    FV   = SIGN(A) == SIGN(tmp8) && SIGN(tmp8) != SIGN(A - tmp8);
    FN   = 1;
    FC   = A < tmp8;
    clock_ticks(3);
    break;

  default:
    fprintf(stderr, "Unknown opcode %02Xh at %04Xh\n", opcode, cpu.pc - 1);
    return -1;
}
