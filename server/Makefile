
# Makefile for /server dir
# Keeping it simple for this time around

CC:=${CROSS_COMPILE}gcc
CFLAGS = -Wall -Wextra -Werror -g
LDFLAGS ?= -lpthread


default: all

all:
	$(CC) $(CFLAGS) $(INCLUDES) aesdsocket.o -o aesdsocket ${LDFLAGS}

clean:
	rm -rf aesdsocket *.o *.elf *.map


.PHONY: all default clean