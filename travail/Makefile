CFLAGS=-Wall
CC=gcc
LDFLAGS=-lpthread
all: client server
	$(CC) -o client $(CFLAGS) client.c;
	$(CC) -o server $(CFLAGS) server.c;
clean:
	rm -f client server