
CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
#CFLAGS = -Wall -g
LDFLAGS = -s

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.o shmem.o
	$(CC) $(LDFLAGS) -shared -Wl -o gkrelldnet.so gkrelldnet.o shmem.o

dnetw: dnetw.o shmem.o
	$(CC) $(LDFLAGS) -o dnetw dnetw.o shmem.o

gkrelldnet.o: gkrelldnet.c
	$(CC) $(CFLAGS) `pkg-config gtk+-2.0 --cflags` -c gkrelldnet.c

install:
	cp gkrelldnet.so $(HOME)/.gkrellm/plugins

clean:
	rm -f gkrelldnet.so dnetw *.o
