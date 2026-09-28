CC = gcc
CFLAGS = -O2 -std=c11 -Wall -Wextra -pedantic

all: sequencial

sequencial: main.c
	$(CC) $(CFLAGS) main.c -o sequencial

clean:
	rm -f sequencial

.PHONY: all clean
