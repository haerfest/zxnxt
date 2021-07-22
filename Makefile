CC=cc
CFLAGS=-Wall -I/usr/local/include -g -Ofast -DDEBUG
LDFLAGS=-lSDL2 -lSDL2_Net

SOURCES=main.c altrom.c audio.c ay.c bootrom.c buffer.c clock.c config.c cpu.c dac.c dma.c divmmc.c esp.c i2c.c io.c joystick.c keyboard.c log.c memory.c mf.c mmu.c mouse.c nextreg.c paging.c rom.c rtc.c sdcard.c slu.c spi.c uart.c utils.c
OBJECTS=$(SOURCES:.c=.o)

all: zxnxt

zxnxt: opcodes.c $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

cpu.o: cpu.c opcodes.c
	$(CC) $(CFLAGS) -c $< -o $@

opcodes.c: opcodes.py
	python3 opcodes.py

slu.o: slu.c layer2.c palette.c sprites.c tilemap.c ula.c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f zxnxt *.o opcodes.c
