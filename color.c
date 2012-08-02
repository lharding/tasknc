/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * color.c - manage curses colors
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* color rule structure */
typedef struct _color_rule
{
	short pair;
	char *rule;
	color_object object;
	struct _color_rule *next;
} color_rule;

/* global variables */
bool use_colors;
bool colors_initialized = false;
bool *pairs_used = NULL;
color_rule *color_rules = NULL;

/* local functions */
static short add_color_pair(const short, const short, const short);
static short find_add_pair(const short, const short);
static int set_default_colors();

short add_color_pair(short askpair, short fg, short bg) /* {{{ */
{
	/* initialize a color pair and return its pair number */
	short pair = 0;

	/* pick a color number if none is specified */
	if (askpair<=0)
	{
		while (pairs_used[pair] && pair<COLOR_PAIRS)
			pair++;
		if (pair == COLOR_PAIRS)
			return -1;
	}

	/* check if pair requested is being used */
	else
	{
		if (pairs_used[askpair])
			return -1;
		pair = askpair;
	}

	/* initialize pair */
	if (init_pair(pair, fg, bg) == ERR)
		return -1;

	/* mark pair as used and exit */
	pairs_used[pair] = true;
	return pair;
} /* }}} */

short add_color_rule(const color_object object, const char *rule, const short fg, const short bg) /* {{{ */
{
	/* add or overwrite a color rule for the provided conditions */
	color_rule *last, *this;
	short ret;

	/* look for existing rule and overwrite colors */
	this = color_rules;
	last = color_rules;
	while (this != NULL)
	{
		if (this->object == object && strcmp(this->rule, rule) == 0)
		{
			ret = find_add_pair(fg, bg);
			if (ret<0)
				return ret;
			this->pair = ret;
			return 0;
		}
		last = this;
		this = this->next;
	}

	/* or create a new rule */
	ret = add_color_pair(-1, fg, bg);
	if (ret<0)
		return ret;
	this = calloc(1, sizeof(color_rule));
	this->pair = ret;
	this->rule = strdup(rule);
	this->object = object;
	this->next = NULL;
	if (last != NULL)
		last->next = this;
	else
		color_rules = this;

	return 0;
} /* }}} */

short find_add_pair(const short fg, const short bg) /* {{{ */
{
	/* find a color pair with specified content or create a new one */
	short *tmpfg = 0, *tmpbg = 0, pair, free_pair = 0;
	int ret;

	/* look for an existing pair */
	for (pair=0; pair<COLOR_PAIRS; pair++)
	{
		if (pairs_used[pair])
		{
			ret = pair_content(pair, tmpfg, tmpbg);
			if (ret == ERR)
				continue;
			if (*tmpfg == fg && *tmpbg == bg)
				return pair;
		}
		else if (free_pair==0)
			free_pair = pair;
	}

	/* return a new pair */
	return add_color_pair(free_pair, fg, bg);
} /* }}} */

void free_colors() /* {{{ */
{
	/* clean up memory allocated for colors */
	color_rule *this, *last;

	check_free(pairs_used);

	this = color_rules;
	while (this != NULL)
	{
		last = this;
		this = this->next;
		check_free(last->rule);
		free(this);
	}
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
		ret = add_color_pair(colors[i].pair, colors[i].fg, colors[i].bg);
		if (ret == -1)
			return 4;
	}

	return 0;
} /* }}} */
