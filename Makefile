#
# makefile for tasknc
#

#variables
CC = gcc
CFLAGS = -Wall -g -Wextra
VERSION = $(shell git describe)

#detect os and handle
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
ifeq ($(uname_O),Cygwin)
		CFLAGS += -I /usr/include/ncurses
endif

all: tasknc doc

doc: tasknc.1
tasknc.1: README.pod
	pod2man --section=1 --center="tasknc Manual" --name="tasknc" --release="tasknc ${VERSION}" $< > $@

tasknc: tasknc.c config.h
		$(CC) $(CFLAGS) tasknc.c -o tasknc -lncurses

tags: tasknc.c config.h
		ctags tasknc.c config.h

clean:
		rm tasknc tasknc.1
