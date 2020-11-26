#!/usr/bin/env python3

instructions = [
    [0xF3, 'cpu.iff1 = 0; clock_tick(4);'],
]


def main():
    with open('opcodes.c', 'w') as f:
        for opcode, code in instructions:
            f.write(f'''
case 0x{opcode:02x}:
  {code}
  break;
''')


if __name__ == '__main__':
    main()
