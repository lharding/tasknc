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
#include "color.h"
#include "command.h"
#include "common.h"
#include "config.h"
#include "keys.h"
#include "log.h"
#include "tasknc.h"

/* local functions */
static void source_fp(const FILE *);

void handle_command(char *cmdstr) /* {{{ */
{
	/* accept a command string, determine what action to take, and execute */
	char *command, *args, *modestr, *pos;
	funcmap *fmap;
	prog_mode mode;
	int ret = 0;

	/* parse args */
	pos = strchr(cmdstr, '\n');
	if (pos != NULL)
		*pos = 0;
	tnc_fprintf(logfp, LOG_DEBUG, "command received: %s", cmdstr);
	if (cmdstr != NULL)
		ret = sscanf(cmdstr, "%ms %m[^\n]", &command, &args);
	if (ret < 1)
	{
		statusbar_message(cfg.statusbar_timeout, "failed to parse command");
		tnc_fprintf(logfp, LOG_ERROR, "failed to parse command: [%d] (%s)", ret, cmdstr);
		return;
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
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: %s - %s (%s)", modestr, command, args);

	/* handle command & arguments */
	/* try for exposed command */
	fmap = find_function(command, mode);
	if (fmap!=NULL)
	{
		(fmap->function)(str_trim(args));
		goto cleanup;
	}
	/* version: print version string */
	if (str_eq(command, "version"))
		statusbar_message(cfg.statusbar_timeout, "%s %s by %s\n", PROGNAME, PROGVERSION, PROGAUTHOR);
	/* quit/exit: exit tasknc */
	else if (str_eq(command, "quit") || str_eq(command, "exit"))
		done = true;
	/* reload: force reload of task list */
	else if (str_eq(command, "reload"))
	{
		reload = true;
		statusbar_message(cfg.statusbar_timeout, "task list reloaded");
	}
	/* redraw: force redraw of screen */
	else if (str_eq(command, "redraw"))
		redraw = true;
	/* dump: write all displayed tasks to log file */
	else if (str_eq(command, "dump"))
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
		statusbar_message(cfg.statusbar_timeout, "error: command %s not found", command);
		tnc_fprintf(logfp, LOG_ERROR, "error: command %s not found", command);
	}
	goto cleanup;

cleanup:
	/* clean up */
	free(command);
	free(args);
} /* }}} */

void run_command_bind(char *args) /* {{{ */
{
	/* create a new keybind */
	int key, ret = 0;
	char *function = NULL, *arg = NULL, *keystr = NULL, *modestr = NULL, *keyname = NULL;
	void (*func)();
	funcmap *fmap;
	prog_mode mode;

	/* parse command */
	if (args != NULL)
		ret = sscanf(args, "%ms %ms %ms %m[^\n]", &modestr, &keystr, &function, &arg);
	if (ret < 3)
	{
		statusbar_message(cfg.statusbar_timeout, "syntax: bind <mode> <key> <function> <args>");
		tnc_fprintf(logfp, LOG_ERROR, "syntax: bind <mode> <key> <function> <args> [%d](%s)", ret, args);
		goto cleanup;
	}

	/* parse mode string */
	if (str_eq(modestr, "tasklist"))
		mode = MODE_TASKLIST;
	else if (str_eq(modestr, "pager"))
		mode = MODE_PAGER;
	else
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: invalid mode (%s)", modestr);
		goto cleanup;
	}

	/* parse key */
	key = parse_key(keystr);

	/* map function to function call */
	fmap = find_function(function, mode);
	if (fmap==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: invalid function specified (%s)", args);
		goto cleanup;
	}
	func = fmap->function;

	/* error out if there is no argument specified when required */
	if (fmap->argn>0 && arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "bind: argument required for function %s", function);
		goto cleanup;
	}

	/* add keybind */
	add_keybind(key, func, arg, mode);
	keyname = name_key(key);
	statusbar_message(cfg.statusbar_timeout, "key %s (%d) bound to %s - %s", keyname, key, modestr, name_function(func));
	goto cleanup;

cleanup:
	free(function);
	free(arg);
	free(keystr);
	free(modestr);
	free(keyname);
	return;
} /* }}} */

void run_command_color(char *args) /* {{{ */
{
	/* create or modify a color rule */
	char *object = NULL, *fg = NULL, *bg = NULL, *rule = NULL;
	color_object obj;
	int ret = 0, fgc, bgc;

	if (args != NULL)
		ret = sscanf(args, "%ms %m[a-z0-9-] %m[a-z0-9-] %m[^\n]", &object, &fg, &bg, &rule);
	if (ret < 3)
	{
		statusbar_message(cfg.statusbar_timeout, "syntax: color <object> <foreground> <background> <rule>");
		tnc_fprintf(logfp, LOG_ERROR, "syntax: color <object> <foreground> <background> <rule>  [%d](%s)", ret, args);
		goto cleanup;
	}

	/* parse object */
	obj = parse_object(object);
	if (obj == OBJECT_NONE)
	{
		statusbar_message(cfg.statusbar_timeout, "color: invalid object \"%s\"", object);
		tnc_fprintf(logfp, LOG_ERROR, "color: invalid object \"%s\"", object);
		goto cleanup;
	}

	/* parse colors */
	fgc = parse_color(fg);
	bgc = parse_color(bg);
	if (bgc < -2 || fgc < -2)
	{
		statusbar_message(cfg.statusbar_timeout, "color: invalid colors \"%s\" \"%s\"", fg, bg);
		tnc_fprintf(logfp, LOG_ERROR, "color: invalid colors %d:\"%s\" %d:\"%s\"", fgc, fg, bgc, bg);
		goto cleanup;
	}

	/* create color rule */
	if (add_color_rule(obj, rule, fgc, bgc)>=0)
		statusbar_message(cfg.statusbar_timeout, "applied color rule");
	else
		statusbar_message(cfg.statusbar_timeout, "applying color rule failed");
	goto cleanup;

cleanup:
	check_free(object);
	check_free(fg);
	check_free(bg);
	check_free(rule);
} /* }}} */

void run_command_unbind(char *argstr) /* {{{ */
{
	/* handle a keyboard instruction to unbind a key */
	char *modestr = NULL, *keystr = NULL, *keyname = NULL;
	prog_mode mode;
	int ret = 0;

	/* parse args */
	if (argstr != NULL)
		ret = sscanf(argstr, "%ms %m[^\n]", &modestr, &keystr);
	if (ret != 2)
	{
		statusbar_message(cfg.statusbar_timeout, "syntax: unbind <mode> <key>");
		tnc_fprintf(logfp, LOG_ERROR, "syntax: unbind <mode> <key> [%d](%s)", ret, argstr);
		goto cleanup;
	}

	/* parse mode */
	if (str_eq(modestr, "pager"))
		mode = MODE_PAGER;
	else if (str_eq(modestr, "tasklist"))
		mode = MODE_TASKLIST;
	else
		mode = MODE_ANY;

	int key = parse_key(keystr);

	remove_keybinds(key, mode);
	keyname = name_key(key);
	statusbar_message(cfg.statusbar_timeout, "key unbound: %s (%d)", keyname, key);
	goto cleanup;

cleanup:
	free(keyname);
	free(modestr);
	free(keystr);
	return;
} /* }}} */

void run_command_set(char *args) /* {{{ */
{
	/* set a variable in the statusbar */
	var *this_var;
	char *message = NULL, *varname = NULL, *value = NULL;
	int ret = 0;

	/* parse args */
	if (args != NULL)
		ret = sscanf(args, "%ms %m[^\n]", &varname, &value);
	if (ret != 2)
	{
		statusbar_message(cfg.statusbar_timeout, "syntax: set <variable> <value>");
		tnc_fprintf(logfp, LOG_ERROR, "syntax: set <variable> <value> [%d](%s)", ret, args);
		goto cleanup;
	}

	/* find the variable */
	this_var = (var *)find_var(varname);
	if (this_var==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "variable not found: %s", varname);
		goto cleanup;
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

cleanup:
	free(message);
	free(varname);
	free(value);
	return;
} /* }}} */

void run_command_show(const char *arg) /* {{{ */
{
	/* display a variable in the statusbar */
	var *this_var;
	char *message = NULL;
	int ret = 0;

	/* parse arg */
	if (arg != NULL)
		ret = sscanf(arg, "%m[^\n]", &message);
	if (ret != 1)
	{
		statusbar_message(cfg.statusbar_timeout, "syntax: show <variable>");
		tnc_fprintf(logfp, LOG_ERROR, "syntax: show <variable> [%d](%s)", ret, arg);
		goto cleanup;
	}

	/* check for a variable */
	if (arg == NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "no variable specified!");
		goto cleanup;
	}

	/* find the variable */
	this_var = (var *)find_var(arg);
	if (this_var==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "variable not found: %s", arg);
		goto cleanup;
	}

	/* acquire the value string and print it */
	message = var_value_message(this_var, 1);
	statusbar_message(cfg.statusbar_timeout, message);

cleanup:
	free(message);
	return;
} /* }}} */

void run_command_source(const char *filepath) /* {{{ */
{
	/* open config file */
	FILE *config = NULL;

	config = fopen(filepath, "r");
	tnc_fprintf(logfp, LOG_DEBUG, "source: file \"%s\"", filepath);

	/* check for a valid fd */
	if (config == NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "source: file \"%s\" could not be opened", filepath);
		statusbar_message(cfg.statusbar_timeout, "source: file \"%s\" could not be opened", filepath);
		return;
	}

	/* read config file */
	source_fp(config);

	/* close config file */
	fclose(config);
	tnc_fprintf(logfp, LOG_DEBUG, "source complete: \"%s\"", filepath);
	statusbar_message(cfg.statusbar_timeout, "source complete: \"%s\"", filepath);
} /* }}} */

void run_command_source_cmd(const char *cmdstr) /* {{{ */
{
	/* open config file */
	FILE *cmd = popen(cmdstr, "r");

	tnc_fprintf(logfp, LOG_DEBUG, "source: command \"%s\"", cmdstr);

	/* check for a valid fd */
	if (cmdstr == NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "source: file \"%s\" could not be opened", cmdstr);
		statusbar_message(cfg.statusbar_timeout, "source: command \"%s\" could not be opened", cmdstr);
		return;
	}

	/* read command file */
	source_fp(cmd);

	/* close config file */
	pclose(cmd);
	tnc_fprintf(logfp, LOG_DEBUG, "source complete: \"%s\"", cmdstr);
	statusbar_message(cfg.statusbar_timeout, "source complete: \"%s\"", cmdstr);
} /* }}} */

void source_fp(const FILE *fp) /* {{{ */
{
	/* given an open file handle, run the commands read from it */
	char *line;

	/* read file */
	line = calloc(TOTALLENGTH, sizeof(char));
	while (fgets(line, TOTALLENGTH, (FILE *)fp))
	{
		char *val;

		/* discard comment lines or blank lines */
		if (line[0]=='#' || line[0]=='\n')
			continue;

		/* handle comments that are mid-line */
		if((val = strchr(line, '#')))
			*val = '\0';

		/* handle commands */
		else
			handle_command(line);

		/* get memory for a new line */
		free(line);
		line = calloc(TOTALLENGTH, sizeof(char));
	}
	free(line);
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
