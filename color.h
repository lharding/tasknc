/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * color.h
 * for tasknc
 * by mjheagle
 */

#ifndef _COLOR_H
#define _COLOR_H

#include <stdio.h>
#include "common.h"

typedef enum
{
	OBJECT_TASK = 0,
	OBJECT_HEADER,
	OBJECT_ERROR,
	OBJECT_NONE,
} color_object;

short add_color_rule(const color_object, const char *, const short, const short);
void free_colors();
int get_colors(const color_object, task *, const bool);
int init_colors();
color_object parse_object(const char *);
int parse_color(const char *);

extern FILE *logfp;
extern task *head;
extern WINDOW *tasklist;

#endif
