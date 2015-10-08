/*
 * statusbar.h
 * for tasknc
 * by mjheagle
 */

#ifndef _STATUSBAR_H
#define _STATUSBAR_H

#include <stdio.h>
#include "common.h"

void free_prompts();
int statusbar_getstr(char**, const char*);
void statusbar_message(const int, const char*, ...) __attribute__((format(printf, 2, 3)));
void statusbar_timeout();

extern config cfg;
extern FILE* logfp;
extern WINDOW* statusbar;

#endif

// vim: noet ts=4 sw=4 sts=4
