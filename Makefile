#
# makefile for tasknc
#

#variables
cc = gcc
cflags = -Wall -g -Wextra

all: tasknc

tasknc: tasknc.c config.h
		$(cc) $(cflags) tasknc.c -o tasknc

clean:
		rm tasknc
