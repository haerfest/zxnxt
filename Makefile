CC=cc
CFLAGS=-Wall -I/usr/local/include -g
LDFLAGS=-lsdl2

SOURCES=main.c altrom.c audio.c bootrom.c clock.c config.c copper.c cpu.c dac.c divmmc.c i2c.c io.c keyboard.c layer2.c log.c memory.c mmu.c nextreg.c palette.c rom.c sdcard.c spi.c timex.c ula.c utils.c
OBJECTS=$(SOURCES:.c=.o)

all: zxnxt

zxnxt: opcodes.c $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

opcodes.c: opcodes.py
	python3 opcodes.py

clean:
	rm -f zxnxt *.o opcodes.c
