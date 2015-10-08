/*
 * color.h
 * for tasknc
 * by mjheagle
 */

#ifndef _COLOR_H
#define _COLOR_H

#include <stdio.h>
#include "common.h"

typedef enum { OBJECT_TASK, OBJECT_HEADER, OBJECT_ERROR, OBJECT_NONE } color_object;

short add_color_rule(const color_object object,
                     const char* rule,
                     const short fg,
                     const short bg);

void free_colors(void);

int get_colors(const color_object object,
               task* tsk,
               const bool selected);

int init_colors(void);

color_object parse_object(const char* name);

int parse_color(const char* name);

extern FILE* logfp;
extern task* head;
extern WINDOW* tasklist;

#endif

// vim: et ts=4 sw=4 sts=4
