# $Id: Makefile,v 1.12 2004-11-27 16:25:37 papier Exp $

CC = gcc
CFLAGS = -Wall -O2 -fPIC -D_GNU_SOURCE
#CFLAGS = -Wall -g
LDFLAGS = -s
VERSION = 0.14.1

all: dnetw gkrelldnet.so

gkrelldnet.so: gkrelldnet.o shmem.o
	$(CC) $(LDFLAGS) -shared -Wl -o $@ $^

dnetw: dnetw.o shmem.o
	$(CC) $(LDFLAGS) -o $@ $^ -lutil

dshm: dshm.o shmem.o
	$(CC) $(LDFLAGS) -o $@ $^

gkrelldnet.o: gkrelldnet.c
	$(CC) $(CFLAGS) `pkg-config gtk+-2.0 --cflags` -c $<

install:
	cp gkrelldnet.so $(HOME)/.gkrellm2/plugins
	@echo "Please read README file for info about dnetw wrapper installation."

clean:
	rm -f gkrelldnet.so dnetw dshm *.o

tar:
	make clean
	(cd ..; tar cvfz gkrelldnet-$(VERSION).tar.gz \
		--exclude './gkrelldnet/CVS' \
		--exclude='./gkrelldnet/*.tar.gz' ./gkrelldnet)
