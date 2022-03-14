CC=gcc
CFLAGS=-Wall -Iincludes -std=gnu99
LDLIBS=-lcrypto
VPATH=src

all: client server test

client: client.c

server: server.c hash.o

hash.o: hash.c

test: test.c hash.o

clean:
	rm -rf client server *.o