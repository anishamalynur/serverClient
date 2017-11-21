CC=g++

CFLAGS=-Wall -W -g -Werror



all: client server

client: client.c raw.c
	gcc client.c raw.c $(CFLAGS) -o client

server: server.c raw.c
	$(CC) server.c raw.c  $(CFLAGS) -o server

clean:
	rm -f client server *.o

