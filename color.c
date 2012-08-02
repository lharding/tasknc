/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * color.c - manage curses colors
 * for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdio.h>
#include "color.h"
#include "common.h"
#include "log.h"

/* color structure */
typedef struct _color
{
	short pair;
	short fg;
	short bg;
	bool initialized;
} color;

/* global variables */
bool use_colors;
bool colors_initialized = false;

/* local functions */
static int set_default_colors();

int init_colors() /* {{{ */
{
	/* initialize curses colors */
	int ret;
	use_colors = false;

	/* attempt to start colors */
	ret = start_color();
	if (ret == ERR)
		return 1;

	/* apply default colors */
	ret = use_default_colors();
	if (ret == ERR)
		return 2;

	/* check if terminal has color capabilities */
	use_colors = has_colors();
	colors_initialized = true;
	if (use_colors)
		return set_default_colors();
	else
		return 3;
} /* }}} */

int set_default_colors() /* {{{ */
{
	/* set up default colors */
	int ret;
	unsigned int i;

	color colors[] =
	{
		{1, COLOR_BLUE,  COLOR_BLACK, 0}, /* title bar */
		{2, COLOR_WHITE, -1,          0}, /* default task */
		{3, COLOR_CYAN,  COLOR_BLACK, 0}, /* selected task */
		{8, COLOR_RED,   -1,          0}, /* error message */
	};

	/* initialize color pairs */
	for (i=0; i<sizeof(colors)/sizeof(color); i++)
	{
		ret = init_pair(colors[i].pair, colors[i].fg, colors[i].bg);
		if (ret == ERR)
			return 4;
		else
			colors[i].initialized = true;
	}

	return 0;
} /* }}} */
