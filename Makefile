# Makefile для ci18n

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./include

.PHONY: all clean example

all: example

example: examples/example.c include/ci18n.h
	$(CC) $(CFLAGS) -o example examples/example.c

clean:
	rm -f example
	rm -f example.exe
