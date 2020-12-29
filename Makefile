CC=cc
CFLAGS=-Wall -I/usr/local/include -DDEBUG
LDFLAGS=-lsdl2

SOURCES=main.c bootrom.c clock.c config.c copper.c cpu.c divmmc.c i2c.c io.c layer2.c log.c memory.c mmu.c nextreg.c rom.c sdcard.c spi.c timex.c ula.c utils.c
OBJECTS=$(SOURCES:.c=.o)

all: twatwa

twatwa: opcodes.c $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

opcodes.c: opcodes.py
	python3 opcodes.py

clean:
	rm -f twatwa *.o opcodes.c
