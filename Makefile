
CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE `glib-config --cflags`
#CFLAGS = -Wall -g `glib-config --cflags`
#LDFLAGS = -s

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.o shmem.o
	$(CC) $(LDFLAGS) -shared -Wl -o gkrelldnet.so gkrelldnet.o shmem.o

dnetw: dnetw.o shmem.o
	$(CC) $(LDFLAGS) -o dnetw dnetw.o shmem.o

gkrelldnet.o: gkrelldnet.c
	$(CC) $(CFLAGS) `imlib-config --cflags-gdk` -c gkrelldnet.c

install:
	cp gkrelldnet.so $(HOME)/.gkrellm/plugins
	cp dnetw /usr/local/bin

clean:
	rm -f gkrelldnet.so dnetw *.o
