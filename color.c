/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * color.c - manage curses colors
 * for tasknc
 * by mjheagle
 */

#include <curses.h>
#include "common.h"
#include "log.h"

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
	use_colors = has_colors() && can_change_color();
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

	ret = init_pair(1, COLOR_BLUE,        COLOR_BLACK);   /* title bar */
	if (ret == ERR)
		return 1;
	ret = init_pair(2, COLOR_WHITE,       -1);            /* default task */
	if (ret == ERR)
		return 1;
	ret = init_pair(3, COLOR_CYAN,        COLOR_BLACK);   /* selected task */
	if (ret == ERR)
		return 1;
	ret = init_pair(8, COLOR_RED,         -1);            /* error message */
	if (ret == ERR)
		return 1;

	return 0;
} /* }}} */
