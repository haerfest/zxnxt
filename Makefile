INCS=-I/usr/local/include
LIBS=-lsdl2

twatwa: main.o clock.o cpu.o io.o memory.o
	cc -Wall $(INCS) $(LIBS) -o twatwa main.o clock.o cpu.o io.o memory.o

main.o: main.c clock.c copper.h cpu.h defs.h
	cc $(INCS) -c main.c

clock.o: clock.c defs.h
	cc $(INCS) -c clock.c

copper.o: copper.c clock.h defs.h
	cc $(INCS) -c copper.c

cpu.o: cpu.c opcodes.c clock.h defs.h
	cc $(INCS) -c cpu.c

io.o: io.c io.h defs.h
	cc $(INCS) -c io.c

opcodes.c: opcodes.py
	python3 opcodes.py

memory.o: memory.c memory.h defs.h
	cc $(INCS) -c memory.c

clean:
	rm *.o
	rm opcodes.c
	rm twatwa
