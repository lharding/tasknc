/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * color.c - manage curses colors
 * for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include "color.h"
#include "common.h"
#include "log.h"

/* color structure */
typedef struct _color
{
	short pair;
	short fg;
	short bg;
} color;

/* global variables */
bool use_colors;
bool colors_initialized = false;
bool *pairs_used = NULL;

/* local functions */
static int set_default_colors();

void free_colors() /* {{{ */
{
	/* clean up memory allocated for colors */
	check_free(pairs_used);
} /* }}} */

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
	{
		/* allocate pairs_used */
		pairs_used = calloc(COLOR_PAIRS, sizeof(bool));
		pairs_used[0] = true;

		return set_default_colors();
	}
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
		{1, COLOR_BLUE,  COLOR_BLACK}, /* title bar */
		{2, COLOR_WHITE, -1},          /* default task */
		{3, COLOR_CYAN,  COLOR_BLACK}, /* selected task */
		{8, COLOR_RED,   -1},          /* error message */
	};

	/* initialize color pairs */
	for (i=0; i<sizeof(colors)/sizeof(color); i++)
	{
		ret = init_pair(colors[i].pair, colors[i].fg, colors[i].bg);
		if (ret == ERR)
			return 4;
		else
			pairs_used[colors[i].pair] = true;
	}

	return 0;
} /* }}} */
