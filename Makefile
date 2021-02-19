CC=gcc
CFLAGS=-Werror=vla -g -Wextra -Wall -Wshadow -Wswitch-default -std=gnu11
CFLAG_SAN=$(CFLAGS) -fsanitize=address
DEPS=
OBJ=hashmap.o

hashmap.o: hashmap.c hashmap.h $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
	
clean:
	rm *.o
