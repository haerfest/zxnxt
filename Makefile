wasbeer: main.o clock.o cpu.o
	cc -Wall -o wasbeer main.o clock.o cpu.o

main.o: main.c clock.c copper.h cpu.h defs.h
	cc -c main.c

clock.o: clock.c defs.h
	cc -c clock.c

copper.o: copper.c clock.h defs.h
	cc -c copper.c

cpu.o: cpu.c clock.h defs.h
	cc -c cpu.c

clean:
	rm *.o
	rm wasbeer
