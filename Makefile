# $Id$

CC = gcc
CFLAGS = -Wall -O2 -fPIC -D_GNU_SOURCE
CFLAGS = -Wall -g -D_GNU_SOURCE
#LDFLAGS = -s
VERSION = 0.14.3

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.o shmem.o
	$(CC) $(LDFLAGS) -shared -Wl -o $@ $^

dnetw: dnetw.o shmem.o
	$(CC) $(LDFLAGS) -o $@ $^ -lutil

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
		--exclude './gkrelldnet/CVS' \
		--exclude='./gkrelldnet/*.tar.gz' ./gkrelldnet)
