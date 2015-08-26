export SYSROOT=$(shell readlink -f `$(CC) -print-prog-name=gcc` | sed -e s!/usr/sh4-linux-uclibc/.*!!)

CC      = sh4-linux-gcc
CFLAGS  = -Wall `sh4-linux-directfb-config --cflags`
LFLAGS  = `sh4-linux-directfb-config --libs` -lasound -lts -lpthread -lm
HEADERS = dfframe.h
OBJS    = dfframe.o

EXECTBL = kadai1

all: $(EXECTBL) tags
	cp $(EXECTBL) /nfs

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXECTBL): main.o $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LFLAGS)

tags: $(EXECTBL)
	ctags -R .

clean:
	rm -f *.o tags $(EXECTBL)
