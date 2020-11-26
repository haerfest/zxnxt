evens = []
for x in range(256):
    even = 1
    for bit in range(8):
        even = even != (x & 1)
        x //= 2
    evens.append(4 * even)

print(', '.join(f'0x{x:02X}' for x in evens))


