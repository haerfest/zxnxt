INCS=-I/usr/local/include
LIBS=-lsdl2

twatwa: main.o clock.o cpu.o divmmc.o io.o memory.o mmu.o nextreg.o
	cc -Wall $(INCS) $(LIBS) -o twatwa main.o clock.o cpu.o divmmc.o io.o memory.o mmu.o nextreg.o

main.o: main.c clock.c copper.h cpu.h defs.h
	cc $(INCS) -c main.c

clock.o: clock.c defs.h
	cc $(INCS) -c clock.c

copper.o: copper.c clock.h defs.h
	cc $(INCS) -c copper.c

cpu.o: cpu.c opcodes.c clock.h defs.h
	cc $(INCS) -c cpu.c

divmmc.o: divmmc.c divmmc.h defs.h
	cc $(INCS) -c divmmc.c

io.o: io.c io.h defs.h
	cc $(INCS) -c io.c

opcodes.c: opcodes.py
	python3 opcodes.py

memory.o: memory.c memory.h defs.h
	cc $(INCS) -c memory.c

mmu.o: mmu.c mmu.h defs.h
	cc $(INCS) -c mmu.c

nextreg.o: nextreg.c nextreg.h defs.h
	cc $(INCS) -c nextreg.c

clean:
	rm *.o
	rm opcodes.c
	rm twatwa
