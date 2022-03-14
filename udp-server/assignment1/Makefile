CC=gcc
CFLAGS=-Wall -Iincludes -std=gnu99
LDLIBS=-lcrypto
VPATH=src

all: client server 

client: client.c

server: server.c 


clean:
	rm -rf client server *.o