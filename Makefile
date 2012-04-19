#
# makefile for tasknc
#

#variables
CC      	= gcc
CFLAGS 		= -Wall -g -Wextra
VERSION 	= $(shell git describe)

PREFIX 	   ?= /usr/local
MANPREFIX  ?= ${PREFIX}/share/man

#detect os and handle
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
ifeq ($(uname_O),Cygwin)
		CFLAGS += -I /usr/include/ncurses
endif

all: tasknc doc

doc: tasknc.1
tasknc.1: README.pod
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

tasknc: tasknc.c config.h
		$(CC) $(CFLAGS) tasknc.c -o tasknc -lncursesw

tags: tasknc.c config.h
		ctags tasknc.c config.h

clean:
		rm tasknc tasknc.1
