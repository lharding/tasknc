#
# makefile for tasknc
#

#variables
cc = gcc
cflags = -Wall -g -Wextra

#detect os and handle
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
ifeq ($(uname_O),Cygwin)
		cflags += -I /usr/include/ncurses
endif

all: tasknc

tasknc: tasknc.c config.h
		$(cc) $(cflags) tasknc.c -o tasknc -lncurses

clean:
		rm tasknc
