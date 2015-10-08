/*
 * statusbar.h
 * for tasknc
 * by mjheagle
 */

#ifndef _STATUSBAR_H
#define _STATUSBAR_H

#include <stdio.h>
#include "common.h"

void free_prompts(void);

int statusbar_getstr(char** str,
                     const char* msg);

void statusbar_message(const int dtmout,
                       const char* format,
                       ...) __attribute__((format(printf, 2, 3)));

void statusbar_timeout(void);

extern config cfg;
extern FILE* logfp;
extern WINDOW* statusbar;

#endif

// vim: et ts=4 sw=4 sts=4
