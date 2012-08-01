/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * command.c - handle commands
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE

#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include "common.h"
#include "keys.h"
#include "log.h"
#include "tasknc.h"

void handle_command(char *cmdstr) /* {{{ */
{
	/* accept a command string, determine what action to take, and execute */
	char *pos, *args = NULL, *modestr;
	cmdstr = str_trim(cmdstr);
	funcmap *fmap;
	prog_mode mode;

	tnc_fprintf(logfp, LOG_DEBUG, "command received: %s", cmdstr);

	/* determine command */
	pos = strchr(cmdstr, ' ');

	/* split off arguments */
	if (pos!=NULL)
	{
		(*pos) = 0;
		args = ++pos;
	}

	/* determine mode */
	if (pager != NULL)
	{
		modestr = "pager";
		mode = MODE_PAGER;
	}
	else if (tasklist != NULL)
	{
		modestr = "tasklist";
		mode = MODE_TASKLIST;
	}
	else
	{
		modestr = "none";
		mode = MODE_ANY;
	}

	/* log command */
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: detected mode %s", modestr);
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: %s", cmdstr);
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: [args] %s", args);

	/* handle command & arguments */
	/* try for exposed command */
	fmap = find_function(cmdstr, mode);
	if (fmap!=NULL)
	{
		(fmap->function)(str_trim(args));
		return;
	}
	/* version: print version string */
	if (str_eq(cmdstr, "version"))
		statusbar_message(cfg.statusbar_timeout, "%s %s by %s\n", PROGNAME, PROGVERSION, PROGAUTHOR);
	/* quit/exit: exit tasknc */
	else if (str_eq(cmdstr, "quit") || str_eq(cmdstr, "exit"))
		done = true;
	/* reload: force reload of task list */
	else if (str_eq(cmdstr, "reload"))
	{
		reload = true;
		statusbar_message(cfg.statusbar_timeout, "task list reloaded");
	}
	/* redraw: force redraw of screen */
	else if (str_eq(cmdstr, "redraw"))
		redraw = true;
	/* dump: write all displayed tasks to log file */
	else if (str_eq(cmdstr, "dump"))
	{
		task *this = head;
		while (this!=NULL)
		{
			tnc_fprintf(logfp, -1, "uuid: %s", this->uuid);
			tnc_fprintf(logfp, -1, "description: %s", this->description);
			tnc_fprintf(logfp, -1, "project: %s", this->project);
			tnc_fprintf(logfp, -1, "tags: %s", this->tags);
			this = this->next;
		}
	}
	else
	{
		statusbar_message(cfg.statusbar_timeout, "error: command %s not found", cmdstr);
		tnc_fprintf(logfp, LOG_ERROR, "error: command %s not found", cmdstr);
	}
} /* }}} */

void run_command_bind(char *args) /* {{{ */
{
	/* create a new keybind */
	int key;
	char *function, *arg, *keystr, *modestr, *aarg, *keyname;
	void (*func)();
	funcmap *fmap;
	prog_mode mode;

	/* make sure an argument was passed */
	if (args==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: mode must be specified (%s)", args);
		return;
	}

	/* split off mode */
	modestr = args;
	keystr = strchr(args, ' ');
	if (keystr==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: key must be specified (%s)", args);
		return;
	}
	(*keystr) = 0;
	keystr++;
	keystr = str_trim(keystr);

	/* parse mode string */
	if (str_eq(modestr, "tasklist"))
		mode = MODE_TASKLIST;
	else if (str_eq(modestr, "pager"))
		mode = MODE_PAGER;
	else
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: invalid mode (%s)", modestr);
		return;
	}

	/* split off key */
	function = strchr(keystr, ' ');
	if (function==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: function must be specified (%s)", args);
		return;
	}
	(*function) = 0;
	function++;
	str_trim(function);

	/* parse key */
	key = parse_key(keystr);

	/* split function from function arg */
	arg = strchr(function, ' ');
	if (arg!=NULL)
	{
		(*arg) = 0;
		arg = str_trim(++arg);
	}

	/* map function to function call */
	fmap = find_function(function, mode);
	if (fmap==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: invalid function specified (%s)", args);
		return;
	}
	func = fmap->function;

	/* error out if there is no argument specified when required */
	if (fmap->argn>0 && arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "bind: argument required for function %s", function);
		return;
	}

	/* copy argument if necessary */
	if (arg!=NULL)
		aarg = strdup(arg);
	else
		aarg = NULL;

	/* add keybind */
	add_keybind(key, func, str_trim(aarg), mode);
	keyname = name_key(key);
	statusbar_message(cfg.statusbar_timeout, "key %s (%d) bound to %s - %s", keyname, key, modestr, name_function(func));
	free(keyname);
	free(aarg);
} /* }}} */

void run_command_unbind(char *argstr) /* {{{ */
{
	/* handle a keyboard instruction to unbind a key */
	char *modestr, *keystr, *keyname;

	/* split strings */
	modestr = argstr;
	if (modestr==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "unbind: mode required");
		return;
	}

	keystr = strchr(argstr, ' ');
	if (keystr==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "unbind: key required");
		return;
	}
	(*keystr) = 0;
	keystr++;

	int key = parse_key(keystr);

	remove_keybinds(key);
	keyname = name_key(key);
	statusbar_message(cfg.statusbar_timeout, "key unbound: %s (%d)", keyname, key);
	free(keyname);
} /* }}} */

void run_command_set(char *args) /* {{{ */
{
	/* set a variable in the statusbar */
	var *this_var;
	char *message, *varname, *value;
	int ret;

	/* check for a variable */
	if (args==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "no variable specified!");
		return;
	}

	/* split the variable and value in the args */
	varname = args;
	value = strchr(args, ' ');
	if (value==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "no value to set %s to!", varname);
		return;
	}
	(*value) = 0;
	value++;

	/* find the variable */
	this_var = (var *)find_var(varname);
	if (this_var==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "variable not found: %s", varname);
		return;
	}

	/* set the value */
	switch (this_var->type)
	{
		case VAR_INT:
			ret = sscanf(value, "%d", (int *)this_var->ptr);
			break;
		case VAR_CHAR:
			ret = sscanf(value, "%c", (char *)this_var->ptr);
			break;
		case VAR_STR:
			if (*(char **)(this_var->ptr)!=NULL)
				free(*(char **)(this_var->ptr));
			while ((*value)==' ')
				value++;
			*(char **)(this_var->ptr) = calloc(strlen(value)+1, sizeof(char));
			ret = NULL!=strcpy(*(char **)(this_var->ptr), value);
			if (ret)
				strip_quotes((char **)this_var->ptr, 1);
			break;
		default:
			ret = 0;
			break;
	}
	if (ret<=0)
		tnc_fprintf(logfp, LOG_ERROR, "failed to parse value from command: set %s %s", varname, value);

	/* acquire the value string and print it */
	message = var_value_message(this_var, 1);
	statusbar_message(cfg.statusbar_timeout, message);
	free(message);
} /* }}} */

void run_command_show(const char *arg) /* {{{ */
{
	/* display a variable in the statusbar */
	var *this_var;
	char *message;

	/* check for a variable */
	if (arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "no variable specified!");
		return;
	}

	/* find the variable */
	this_var = (var *)find_var(arg);
	if (this_var==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "variable not found: %s", arg);
		return;
	}

	/* acquire the value string and print it */
	message = var_value_message(this_var, 1);
	statusbar_message(cfg.statusbar_timeout, message);
	free(message);
} /* }}} */

void strip_quotes(char **strptr, bool needsfree) /* {{{ */
{
	/* remove leading/trailing quotes from a string if necessary */
	const char *quotes = "\"'";
	char *newstr, *end, *str = *strptr;
	bool inquotes = false;
	int len = 0;

	/* walk to end of string */
	end = str;
	while (*(end+1) != 0)
	{
		len++;
		end++;
	}

	/* determine if string has quotes */
	inquotes = *str == *end && strchr(quotes, *str) != NULL;

	/* copy quoted string */
	if (inquotes)
	{
		newstr = calloc(len+1, sizeof(char));
		strncpy(newstr, str+1, len-1);
		*strptr = newstr;
	}

	/* copy unquoted string */
	else
		*strptr = strdup(str);

	/* free if necessary */
	if (needsfree)
		free(str);
} /* }}} */
