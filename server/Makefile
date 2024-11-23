INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -Wall -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= aesdsocket.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	${CFILES}
	$(CC) $(LDFLAGS) $(CFLAGS) -o aesdsocket aesdsocket.c -lpthread -lrt

default:
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt

clean:
	-rm -f *.o *.d

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

