/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * keys.c - handle keyboard input
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "keys.h"
#include "log.h"
#include "tasklist.h"
#include "tasknc.h"

void add_int_keybind(const int key, void *function, const int argint, const prog_mode mode) /* {{{ */
{
	/* convert integer to string, then add keybind */
	char *argstr;

	asprintf(&argstr, "%d", argint);
	add_keybind(key, function, argstr, mode);
	free(argstr);
} /* }}} */

void add_keybind(const int key, void *function, char *arg, const prog_mode mode) /* {{{ */
{
	/* add a keybind to the list of binds */
	keybind *this_bind, *new;
	int n = 0;
	char *modestr;

	/* create new bind */
	new = calloc(1, sizeof(keybind));
	new->key = key;
	new->function = function;
	new->argint = 0;
	new->argstr = NULL;
	new->argstr = arg;
	new->next = NULL;
	new->mode = mode;

	/* append it to the list */
	if (keybinds==NULL)
		keybinds = new;
	else
	{
		this_bind = keybinds;
		for (n=0; this_bind->next!=NULL; n++)
			this_bind = this_bind->next;
		this_bind->next = new;
		n++;
	}

	/* write log */
	if (mode == MODE_PAGER)
		modestr = "pager - ";
	else if (mode == MODE_TASKLIST)
		modestr = "tasklist - ";
	else
		modestr = " ";
	tnc_fprintf(logfp, LOG_DEBUG, "bind #%d: key %c (%d) bound to @%p %s%s(args: %d/%s)", n, key, key, function, modestr, name_function(function), new->argint, new->argstr);
} /* }}} */

int parse_key(const char *keystr) /* {{{ */
{
	/* determine a key value from a string specifier */
	int key;

	/* try for integer key */
	if (1==sscanf(keystr, "%d", &key))
		return key;

	/* take the first character as the key */
	return (int)(*keystr);

} /* }}} */

int remove_keybinds(const int key) /* {{{ */
{
	/* remove all keybinds to key */
	int counter = 0;
	keybind *this, *last = NULL, *next;

	this = keybinds;
	while (this!=NULL)
	{
		next = this->next;
		if (this->key == (int)key)
		{
			if (last!=NULL)
				last->next = next;
			else
				keybinds = next;
			free(this);
			counter++;
		}
		else
			last = this;
		this = next;
	}

	return counter;
} /* }}} */
