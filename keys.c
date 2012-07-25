/*
 * keys.c - handle keyboard input
 * for tasknc
 * by mjheagle
 *
 * vim: noet ts=4 sw=4 sts=4
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "keys.h"
#include "log.h"

/* todo: change this */
#include <ncurses.h>
#include "tasknc.h"

keybind *keybinds = NULL;

void add_int_keybind(int key, void *function, int argint) /* {{{ */
{
	/* convert integer to string, then add keybind */
	char *argstr;

	asprintf(&argstr, "%d", argint);
	add_keybind(key, function, argstr);
	free(argstr);
} /* }}} */

void add_keybind(int key, void *function, char *arg) /* {{{ */
{
	/* add a keybind to the list of binds */
	keybind *this_bind, *new;
	int n = 0;

	/* create new bind */
	new = calloc(1, sizeof(keybind));
	new->key = key;
	new->function = function;
	new->argint = 0;
	new->argstr = NULL;
	if (function==key_task_action)
		new->argint = atoi(arg);
	else if (function==key_scroll)
		new->argint = *arg;
	else
		new->argstr = arg;
	new->next = NULL;

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
	tnc_fprintf(logfp, LOG_DEBUG, "bind #%d: key %c (%d) bound to @%p (args: %d/%s)", n, key, key, function,new->argint, new->argstr);
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
