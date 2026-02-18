# Makefile для ci18n

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./include

.PHONY: all clean example test

all: example test

example: examples/example.c include/ci18n.h
	$(CC) $(CFLAGS) -o example examples/example.c

test: tests/test_ci18n.c include/ci18n.h
	$(CC) $(CFLAGS) -o test_ci18n tests/test_ci18n.c
	./test_ci18n

clean:
	del /q example.exe test_ci18n.exe 2>nul || rm -f example test_ci18n
