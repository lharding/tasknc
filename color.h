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
} color_object;

short add_color_rule(const color_object, const char *, const short, const short);
void free_colors();
int get_colors(const color_object, const task *, const task *);
int init_colors();

extern FILE *logfp;

#endif
