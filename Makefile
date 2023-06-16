# This is a comment line
CC=gcc
# CFLAGS will be the options passed to the compiler.
CFLAGS= -c -Wall
all: output clean

output: main.o utils.o copy.o
	$(CC) main.o utils.o copy.o -o output

main.o: main.c
	$(CC) $(CFLAGS) main.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) utils.c

copy.o: copy.c copy.h
	$(CC) $(CFLAGS) copy.c

target: dependencies
	action

clean:
	rm -f *.o
