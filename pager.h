/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * view.h
 * for tasknc
 * by mjheagle
 */


#ifndef _VIEW_H
#define _VIEW_H

#include <stdbool.h>
#include "common.h"

typedef struct _line
{
	char *str;
	struct _line *next;
} line;

void free_lines(line *);
void help_window();
void pager_command(const char *, const char *, const bool, const int, const int);
void view_stats();
void view_task(task *);

extern bool redraw;
extern config cfg;
extern FILE *logfp;
extern int cols;
extern int rows;
extern WINDOW *statusbar;

#endif
