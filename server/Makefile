
# Makefile for /server dir
# Keeping it simple for this time around

CC ?= ${CROSS_COMPILE}gcc
CFLAGS ?= -Wall -Wextra -Werror -g
LDFLAGS ?= -lpthread
.PHONY: default all


all: default

default:aesdsocket

$(TARGET_AESDSOCKET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) aesdsocket.o -o aesdsocket $(LDFLAGS)

$(TARGET_AESDSOCKET).o: $(TARGET_AESDSOCKET).c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f aesdsocket *.o *.elf *.map
