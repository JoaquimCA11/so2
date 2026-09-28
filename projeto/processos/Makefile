CC = gcc
CFLAGS = -O2 -std=c11 -Wall -Wextra -pedantic -pthread

all: processos

processos: main.c
	$(CC) $(CFLAGS) main.c -o processos

clean:
	rm -f processos

.PHONY: all clean
