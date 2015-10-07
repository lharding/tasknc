#
# makefile for tasknc
#
OUT 		= tasknc

#variables
CC		?= cc
CFLAGS 		?= -Wall -g -Wextra -std=c99 -O2
LDFLAGS 	= ${CFLAGS}
LDLIBS 		= -lncursesw
VERSION 	= $(shell git describe)

PREFIX 	   ?= /usr/local
MANPREFIX  ?= ${PREFIX}/share/man
MANPAGE = $(OUT).1

SRCDIR = src
INCDIR = include

CFLAGS += -I $(INCDIR)

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst %.c,%.o,$(SRC))

#detect os and handle
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
ifeq ($(uname_O),Cygwin)
		CFLAGS += -I /usr/include/ncurses
endif

all: $(OUT) doc

doc: $(MANPAGE)
$(MANPAGE): doc/manual.pod
		pod2man --errors=stderr --section=1 --center="tasknc Manual" --name="tasknc" --release="tasknc ${VERSION}" $< > $@

install: $(OUT) $(MANPAGE)
		install -D -m755 tasknc ${DESTDIR}${PREFIX}/bin/$(OUT)
		install -D -m644 tasknc.1 ${DESTDIR}${MANPREFIX}/man1/$(MANPAGE)
		install -D -m644 config ${DESTDIR}${PREFIX}/share/tasknc/config

uninstall:
		@echo removing executable file from ${DESTDIR}${PREFIX}/bin
		rm -f ${DESTDIR}${PREFIX}/bin/$(OUT)
		@echo removing man page from ${DESTDIR}${PREFIX}/man1/$(MANPAGE)
		rm -f ${DESTDIR}/${PREFIX}/man1/$(MANPAGE)

$(OBJ): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

tags: $(SRC)
		ctags $(SRC) $(wildcard $(INCDIR)/*.h)

clean:
		rm $(OUT) $(MANPAGE) $(OBJ) tags || true
