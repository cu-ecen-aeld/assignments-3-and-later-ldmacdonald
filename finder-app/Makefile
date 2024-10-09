ifeq ($(CROSS_COMPILE),)
	CC = gcc
else
	CC=$(CROSS_COMPILE)gcc
endif

all:
	$(CC) -o writer writer.c

writer: writer.c
	$(CC) -o writer writer.c

clean:
	rm -f writer
	rm -f *.o
