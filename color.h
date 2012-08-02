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

typedef enum
{
	OBJECT_TASK = 0,
	OBJECT_HEADER,
} color_object;

void free_colors();
int init_colors();

extern FILE *logfp;

#endif
