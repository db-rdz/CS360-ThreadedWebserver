CC = gcc
CFLAGS = -g

all: server

server: config-parse.c config-parse.h tcp-server.c http-parser.h http-parser.c main.c
	$(CC) $(CFLAGS) -o server config-parse.c config-parse.h tcp-server.c http-parser.h http-parser.c main.c -lm

clean:
	rm -f server
