CC=gcc
CFLAGS=-Wall
all: amdctl
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 
