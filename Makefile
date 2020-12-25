INCS=-I/usr/local/include
LIBS=-lsdl2

twatwa: main.o bootrom.o clock.o config.o copper.o cpu.o divmmc.o i2c.o io.o layer2.o memory.o mmu.o nextreg.o rom.o sdcard.o spi.o timex.o ula.o utils.o
	cc -Wall $(INCS) $(LIBS) -o twatwa main.o bootrom.o clock.o config.o copper.o cpu.o divmmc.o i2c.o io.o layer2.o memory.o mmu.o nextreg.o rom.o sdcard.o spi.o timex.o ula.o utils.o

main.o: main.c cpu.h defs.h
	cc $(INCS) -c main.c

bootrom.o: bootrom.c bootrom.h defs.h
	cc $(INCS) -c bootrom.c

clock.o: clock.c clock.h defs.h
	cc $(INCS) -c clock.c

config.o: config.c config.h defs.h
	cc $(INCS) -c config.c

copper.o: copper.c copper.h defs.h
	cc $(INCS) -c copper.c

cpu.o: cpu.c opcodes.c cpu.h defs.h
	cc $(INCS) -c cpu.c

divmmc.o: divmmc.c divmmc.h defs.h
	cc $(INCS) -c divmmc.c

i2c.o: i2c.c i2c.h defs.h
	cc $(INCS) -c i2c.c

io.o: io.c io.h defs.h
	cc $(INCS) -c io.c

layer2.o: layer2.c layer2.h defs.h
	cc $(INCS) -c layer2.c

memory.o: memory.c memory.h defs.h
	cc $(INCS) -c memory.c

mmu.o: mmu.c mmu.h defs.h
	cc $(INCS) -c mmu.c

nextreg.o: nextreg.c nextreg.h defs.h
	cc $(INCS) -c nextreg.c

opcodes.c: opcodes.py
	python3 opcodes.py

rom.o: rom.c rom.h defs.h
	cc $(INCS) -c rom.c

sdcard.o: sdcard.c sdcard.h defs.h
	cc $(INCS) -c sdcard.c

spi.o: spi.c spi.h defs.h
	cc $(INCS) -c spi.c

timex.o: timex.c timex.h defs.h
	cc $(INCS) -c timex.c

ula.o: ula.c ula.h defs.h
	cc $(INCS) -c ula.c

utils.o: utils.c utils.h defs.h
	cc $(INCS) -c utils.c

clean:
	rm *.o
	rm opcodes.c
	rm twatwa
