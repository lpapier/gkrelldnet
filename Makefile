
CFLAGS = -O2 `glib-config --cflags --libs`
#CFLAGS = -O -g `glib-config --cflags --libs`

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.c
	gcc -O2 -fPIC `gtk-config --cflags` `imlib-config --cflags-gdk` -c gkrelldnet.c
	gcc -shared -Wl -o gkrelldnet.so gkrelldnet.o

dnetw: dnetw.c
	gcc $(CFLAGS) $(LDFLAGS) -o dnetw dnetw.c

install:
	cp gkrelldnet.so $(HOME)/.gkrellm/plugins

clean:
	rm -f gkrelldnet.so gkrelldnet.o dnetw dnetw.o
