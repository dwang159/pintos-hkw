CC = gcc
CFLAGS = -Wall -W -std=c99 -g -O0
LDFLAGS= -lreadline

all: mysh

mysh: mysh.o tokenize.o command.o execute.o internal.o

test: test_mysh.o tokenize.o command.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o test test_mysh.o tokenize.o command.o
	./test

clean:
	rm -f *.o *~ mysh test

.PHONY: all clean

