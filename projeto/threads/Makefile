CC = gcc
CFLAGS = -O2 -std=c11 -Wall -Wextra -pedantic -pthread

all: threads

threads: main.c
	$(CC) $(CFLAGS) main.c -o threads

clean:
	rm -f threads

.PHONY: all clean
