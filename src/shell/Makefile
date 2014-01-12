CC = gcc
CFLAGS = -Wall -W
LDFLAGS= -lreadline

all: mysh

mysh: mysh.o tokenize.o

test: test_mysh.o tokenize.o
	$(CC) $(CFLAGS) -o test test_mysh.o tokenize.o
	./test

clean: 
	rm -f *.o *~ mysh test

.PHONY: all clean

