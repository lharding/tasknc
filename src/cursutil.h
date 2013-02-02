/*
 * ncurses utilities for tasknc
 * by mjheagle
 */


#ifndef _TASKNC_CURSUTIL_H
#define _TASKNC_CURSUTIL_H

#include <curses.h>

int umvaddstr(WINDOW *, const int, const int, const int, const char *, ...);
int umvaddstr_align(WINDOW *, const int, const int, char *);

#endif
