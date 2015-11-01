CC=gcc
CFLAGS=-Wall -pedantic -Wextra
all: amdctl
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 
