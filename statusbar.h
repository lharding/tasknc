/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * statusbar.h
 * for tasknc
 * by mjheagle
 */

#ifndef _STATUSBAR_H
#define _STATUSBAR_H

#include <stdio.h>
#include "common.h"

void statusbar_message(const int, const char *, ...) __attribute__((format(printf,2,3)));
void statusbar_timeout();

extern config cfg;
extern FILE *logfp;
extern WINDOW *statusbar;

#endif
