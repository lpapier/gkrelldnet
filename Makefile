# $Id$

CC = gcc
CFLAGS = -Wall -O2 -fPIC -D_GNU_SOURCE
LDFLAGS = -s

#CFLAGS = -Wall -g -D_GNU_SOURCE -DHAVE_LIBMICROHTTPD

## uncomment the following lines to use libmicrohttpd 98912660
#CFLAGS = -Wall -O2 -fPIC -D_GNU_SOURCE -DHAVE_LIBMICROHTTPD
#LIBS = -lmicrohttpd

VERSION = 0.15

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.o shmem.o dprint.o
	$(CC) $(LDFLAGS) -shared -Wl -o $@ $^

dnetw: dnetw.o shmem.o dprint.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lutil

dshm: dshm.o shmem.o
	$(CC) $(LDFLAGS) -o $@ $^

dnetweb: dnetweb.o shmem.o
	$(CC) $(LDFLAGS) -o $@ $^ -lmicrohttpd

gkrelldnet.o: gkrelldnet.c
	$(CC) $(CFLAGS) `pkg-config gtk+-2.0 --cflags` -c $<

install:
	cp gkrelldnet.so $(HOME)/.gkrellm2/plugins
	@echo "Please read README file for info about dnetw wrapper installation."

clean:
	rm -f gkrelldnet.so dnetw dshm dnetweb *.o

tar:
	make clean
	(cd ..; tar cvfz gkrelldnet-$(VERSION).tar.gz \
		--exclude './gkrelldnet/.svn' \
		--exclude='./gkrelldnet/*.tar.gz' ./gkrelldnet)
