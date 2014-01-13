CC = gcc
CFLAGS = -Wall -W
LDFLAGS= -lreadline

all: mysh

mysh: mysh.o tokenize.o command.o

test: test_mysh.o tokenize.o command.o
	$(CC) $(CFLAGS) -o test test_mysh.o tokenize.o
	./test

clean: 
	rm -f *.o *~ mysh test

.PHONY: all clean

