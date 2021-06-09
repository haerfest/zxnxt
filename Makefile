CC=cc
CFLAGS=-Wall -I/usr/local/include -g -Ofast -DDEBUG
LDFLAGS=-lSDL2

SOURCES=main.c altrom.c audio.c ay.c bootrom.c clock.c config.c copper.c cpu.c dac.c dma.c divmmc.c i2c.c io.c kempston.c keyboard.c layer2.c log.c memory.c mf.c mmu.c nextreg.c palette.c paging.c rom.c rtc.c sdcard.c slu.c spi.c sprites.c tilemap.c ula.c utils.c
OBJECTS=$(SOURCES:.c=.o)

all: zxnxt

zxnxt: opcodes.c $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

ula.o: ula.c ula_mode_x.c ula_hi_res.c
	$(CC) $(CFLAGS) -c $< -o $@

cpu.o: cpu.c opcodes.c
	$(CC) $(CFLAGS) -c $< -o $@

opcodes.c: opcodes.py
	python3 opcodes.py

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f zxnxt *.o opcodes.c
