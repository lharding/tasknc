#
# makefile for tasknc
#
OUT 		= tasknc

#variables
CC      	= cc
CFLAGS 		= -Wall -g -Wextra -std=c99 -O2
LDFLAGS 	= ${CFLAGS} -lncursesw
VERSION 	= $(shell git describe)

PREFIX 	   ?= /usr/local
MANPREFIX  ?= ${PREFIX}/share/man

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

#detect os and handle
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
ifeq ($(uname_O),Cygwin)
		CFLAGS += -I /usr/include/ncurses
endif

all: $(OUT) doc

doc: tasknc.1
tasknc.1: doc/manual.pod
		pod2man --section=1 --center="tasknc Manual" --name="tasknc" --release="tasknc ${VERSION}" $< > $@

install: tasknc tasknc.1
		install -D -m755 tasknc ${DESTDIR}${PREFIX}/bin/tasknc
		install -D -m644 tasknc.1 ${DESTDIR}${MANPREFIX}/man1/tasknc.1
		install -D -m644 config ${DESTDIR}${PREFIX}/share/tasknc/config

uninstall:
		@echo removing executable file from ${DESTDIR}${PREFIX}/bin
		rm -f ${DESTDIR}${PREFIX}/bin/tasknc
		@echo removing man page from ${DESTDIR}${PREFIX}/man1/tasknc.1
		rm -f ${DESTDIR}/${PREFIX}/man1/tasknc.1

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OUT): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

tags: $(SRC)
		ctags *.c *.h

clean:
		rm tasknc tasknc.1 *.o
