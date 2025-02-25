
# Makefile for /server dir
# Keeping it simple for this time around

CC ?= ${CROSS_COMPILE}gcc
CFLAGS ?= -Wall -Wextra -Werror -g
LDFLAGS ?= -lpthread


default: all

all:aesdsocket

aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) $(INCLUDES) aesdsocket.o -o aesdsocket $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f aesdsocket *.o *.elf *.map


.PHONY: clean