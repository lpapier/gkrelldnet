
CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE `glib-config --cflags`
#CFLAGS = -Wall -O -g `glib-config --cflags`
#LDFLAGS = -s

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.c
	$(CC) $(CFLAGS) `imlib-config --cflags-gdk` -c gkrelldnet.c
	$(CC) $(LDFLAGS) -shared -Wl -o gkrelldnet.so gkrelldnet.o

dnetw: dnetw.o
	$(CC) $(LDFLAGS) -o dnetw dnetw.o

install:
	cp gkrelldnet.so $(HOME)/.gkrellm/plugins

clean:
	rm -f gkrelldnet.so gkrelldnet.o dnetw dnetw.o
