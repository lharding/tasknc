/*
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 *
 * vim: noet ts=4 sw=4 sts=4
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <curses.h>
#include <getopt.h>
#include <locale.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "common.h"
#include "config.h"
#include "tasknc.h"
#include "tasks.h"
#include "log.h"

/* run state structs {{{ */
struct {
	short project;
	short description;
	short date;
} fieldlengths;

struct {
	char *title;
	char *task;
} formats;

struct {
	bool redraw;
	bool reload;
	bool done;
} state;
/* }}} */

/* global variables {{{ */
const char *progname = "tasknc";
const char *progversion = "v0.6";
const char *progauthor = "mjheagle";

config cfg;                             /* runtime config struct */
short pageoffset = 0;                   /* number of tasks page is offset */
time_t sb_timeout = 0;                  /* when statusbar should be cleared */
char *searchstring = NULL;              /* currently active search string */
short selline = 0;                      /* selected line number */
int rows, cols;                         /* size of the ncurses window */
int taskcount;                          /* number of tasks */
int totaltaskcount;                     /* number of tasks with no filters applied */
char *active_filter = NULL;             /* a string containing the active filter string */
task *head = NULL;                      /* the current top of the list */
FILE *logfp;                            /* handle for log file */
keybind *keybinds = NULL;               /* linked list of keybinds */
/* }}} */

/* user-exposed variables & functions {{{ */
var vars[] = {
	{"ncurses_timeout",   VAR_INT,  &(cfg.nc_timeout)},
	{"statusbar_timeout", VAR_INT,  &(cfg.statusbar_timeout)},
	{"log_level",         VAR_INT,  &(cfg.loglvl)},
	{"task_version",      VAR_STR,  &(cfg.version)},
	{"sort_mode",         VAR_CHAR, &(cfg.sortmode)},
	{"search_string",     VAR_STR,  &searchstring},
	{"selected_line",     VAR_INT,  &selline},
	{"task_count",        VAR_INT,  &totaltaskcount},
	{"filter_string",     VAR_STR,  &active_filter},
	{"silent_shell",      VAR_CHAR, &(cfg.silent_shell)},
	{"program_name",      VAR_STR,  &progname},
	{"program_author",    VAR_STR,  &progauthor},
	{"program_version",   VAR_STR,  &progversion},
	{"title_format",      VAR_STR,  &(formats.title)},
	{"task_format",       VAR_STR,  &(formats.task)},
};

funcmap funcmaps[] = {
	{"scroll",      (void *)key_scroll,      1},
	{"task_action", (void *)key_task_action, 1},
	{"reload",      (void *)key_reload,      0},
	{"undo",        (void *)key_undo,        0},
	{"add",         (void *)key_add,         0},
	{"modify",      (void *)key_modify,      0},
	{"sort",        (void *)key_sort,        0},
	{"search",      (void *)key_search,      0},
	{"search_next", (void *)key_search_next, 0},
	{"filter",      (void *)key_filter,      0},
	{"sync",        (void *)key_sync,        0},
	{"quit",        (void *)key_done,        0},
	{"command",     (void *)key_command,     0}
};
/* }}} */

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

void check_curs_pos(void) /* {{{ */
{
	/* check if the cursor is in a valid position */
	const short onscreentasks = rows-3;

	/* check for a valid selected line number */
	if (selline<0)
		selline = 0;
	else if (selline>=taskcount)
		selline = taskcount-1;

	/* check if page offset needs to be changed */
	if (selline<pageoffset)
		pageoffset = selline;
	else if (selline>pageoffset+onscreentasks)
		pageoffset = selline - onscreentasks;

	/* log cursor position */
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "selline:%d offset:%d taskcount:%d perscreen:%d", selline, pageoffset, taskcount, rows-3);
} /* }}} */

void check_screen_size() /* {{{ */
{
	/* check for a screen thats too small */
	int count = 0;

	do
	{
		if (count)
		{
			if (count==1)
			{
				wipe_statusbar();
				wipe_tasklist();
			}
			attrset(COLOR_PAIR(8));
			mvaddstr(0, 0, "screen dimensions too small");
			wrefresh(stdscr);
			attrset(COLOR_PAIR(0));
			usleep(100000);
		}
		count++;
		getmaxyx(stdscr, rows, cols);
	} while (cols<DATELENGTH+20+fieldlengths.project || rows<5);
} /* }}} */

void cleanup() /* {{{ */
{
	/* function to run on termination */
	keybind *lastbind;

	/* free memory allocated normally */
	if (searchstring!=NULL)
		free(searchstring);
	free_tasks(head);
	free(cfg.version);
	while (keybinds!=NULL)
	{
		lastbind = keybinds;
		keybinds = keybinds->next;
		free(lastbind);
	}

	/* close open files */
	fflush(logfp);
	fclose(logfp);
} /* }}} */

void configure(void) /* {{{ */
{
	/* parse config file to get runtime options */
	FILE *cmd, *config;
	char line[TOTALLENGTH], *filepath, *xdg_config_home, *home;
	int ret;

	/* set default settings */
	cfg.nc_timeout = NCURSES_WAIT;                          /* time getch will wait */
	cfg.statusbar_timeout = STATUSBAR_TIMEOUT_DEFAULT;      /* default time before resetting statusbar */
	cfg.sortmode = 'd';                                     /* determine sort algorithm */
	cfg.silent_shell = 0;                                   /* determine whether shell commands should be visible */

	/* set default formats */
	formats.title = calloc(64, sizeof(char));
	strcpy(formats.title, " $program_name ($selected_line/$task_count) $> $date ");
	formats.task = calloc(64, sizeof(char));
	strcpy(formats.task, " $project $description $> $due ");

	/* get task version */
	cmd = popen("task version rc._forcecolor=no", "r");
	while (fgets(line, sizeof(line)-1, cmd) != NULL)
	{
		cfg.version = calloc(8, sizeof(char));
		ret = sscanf(line, "task %[^ ]* ", cfg.version);
		if (ret>0)
		{
			tnc_fprintf(logfp, LOG_DEBUG, "task version: %s", cfg.version);
			break;
		}
	}
	pclose(cmd);

	/* default keybinds */
	add_keybind(ERR,           NULL,                 NULL);
	add_keybind('k',           key_scroll,           "u");
	add_keybind(KEY_UP,        key_scroll,           "u");
	add_keybind('j',           key_scroll,           "d");
	add_keybind(KEY_DOWN,      key_scroll,           "d");
	add_keybind(KEY_HOME,      key_scroll,           "h");
	add_keybind('g',           key_scroll,           "h");
	add_keybind(KEY_END,       key_scroll,           "e");
	add_keybind('G',           key_scroll,           "e");
	add_int_keybind('e',       key_task_action,      ACTION_EDIT);
	add_keybind('r',           key_reload,           NULL);
	add_keybind('u',           key_undo,             NULL);
	add_int_keybind('d',       key_task_action,      ACTION_DELETE);
	add_int_keybind('c',       key_task_action,      ACTION_COMPLETE);
	add_keybind('a',           key_add,              NULL);
	add_int_keybind('v',       key_task_action,      ACTION_VIEW);
	add_int_keybind(13,        key_task_action,      ACTION_VIEW);
	add_int_keybind(KEY_ENTER, key_task_action,      ACTION_VIEW);
	add_keybind('s',           key_sort,             NULL);
	add_keybind('/',           key_search,           NULL);
	add_keybind('n',           key_search_next,      NULL);
	add_keybind('f',           key_filter,           NULL);
	add_keybind('y',           key_sync,             NULL);
	add_keybind('q',           key_done,             NULL);
	add_keybind(';',           key_command,          NULL);
	add_keybind(':',           key_command,          NULL);

	/* determine config path */
	xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home == NULL)
	{
		home = getenv("HOME");
		filepath = malloc((strlen(home)+25)*sizeof(char));
		sprintf(filepath, "%s/.config/tasknc/config", home);
	}
	else
	{
		filepath = malloc((strlen(xdg_config_home)+16)*sizeof(char));
		sprintf(filepath, "%s/tasknc/config", xdg_config_home);
	}

	/* open config file */
	config = fopen(filepath, "r");
	tnc_fprintf(logfp, LOG_DEBUG, "config file: %s", filepath);

	/* free filepath */
	free(filepath);

	/* check for a valid fd */
	if (config == NULL)
	{
		tnc_fprintf(stdout, LOG_ERROR, "config file could not be opened");
		tnc_fprintf(logfp, LOG_ERROR, "config file could not be opened");
		return;
	}

	/* read config file */
	tnc_fprintf(logfp, LOG_DEBUG, "reading config file");
	while (fgets(line, TOTALLENGTH, config))
	{
		char *val;

		/* discard comment lines or blank lines */
		if (line[0]=='#' || line[0]=='\n')
			continue;

		/* handle comments that are mid-line */
		if((val = strchr(line, '#')))
			*val = '\0';

		/* handle config commands */
		else
			handle_command(line);
	}

	/* close config file */
	fclose(config);
} /* }}} */

const char *eval_string(const int maxlen, const char *fmt, const task *this, char *str, int position) /* {{{ */
{
	/* evaluate a string with format variables */
	int i;
	char *tmp;

	/* check if string is done */
	if (*fmt == 0)
		return str;

	/* allocate string if necessary */
	if (str==NULL)
		str = calloc(maxlen+1, sizeof(char));

	/* check if this is a variable */
	if (*fmt != '$')
	{
		str[position] = *fmt;
		return eval_string(maxlen, ++fmt, this, str, ++position);
	}

	/* try for a special format string */
	if (str_starts_with(fmt+1, "date"))
	{
		char *msg = utc_date(0);
		strcpy(str+position, msg);
		const int msglen = strlen(msg);
		free(msg);
		return eval_string(maxlen, fmt+5, this, str, position+msglen);
	}

	/* try for a variable relative to the current task */
	if (this!=NULL)
	{
		int fieldlen = 0;
		int fieldwidth = 0;
		int varlen = 0;
		if (str_starts_with(fmt+1, "due"))
		{
			/* fieldwidth = fieldlengths.date; */
			if (this->due)
			{
				tmp = utc_date(this->due);
				strcpy(str+position, tmp);
				fieldlen = strlen(tmp);
				free(tmp);
			}
			else if (this->priority)
			{
				*(str+position) = this->priority;
				fieldlen = 1;
			}
			else
				fieldlen = 0;
			varlen = strlen("due");
			fieldwidth = fieldlen;
		}
		else if (str_starts_with(fmt+1, "description"))
		{
			strcpy(str+position, this->description);
			fieldwidth = fieldlengths.description;
			fieldlen = strlen(this->description);
			varlen = strlen("description");
		}
		else if (str_starts_with(fmt+1, "project"))
		{
			strcpy(str+position, this->project);
			fieldwidth = fieldlengths.project;
			fieldlen = strlen(this->project);
			varlen = strlen("project");
		}
		for (i=fieldlen; i<fieldwidth; i++)
			*(str+position+i) = ' ';
		if (varlen>0)
			return eval_string(maxlen, fmt+varlen+1, this, str, position+fieldwidth);
	}

	/* try for an exposed variable */
	for (i=0; i<NVARS; i++)
	{
		if (str_starts_with(fmt+1, vars[i].name))
		{
			char *msg = var_value_message(&(vars[i]), 0);
			strcpy(str+position, msg);
			const int msglen = strlen(msg);
			free(msg);
			return eval_string(maxlen, fmt+strlen(vars[i].name)+1, this, str, position+msglen);
		}
	}

	/* print uninterpreted */
	str[position] = *fmt;
	return eval_string(maxlen, ++fmt, this, str, ++position);
} /* }}} */

funcmap *find_function(const char *name) /* {{{ */
{
	/* search through the function maps */
	int i;

	for (i=0; i<NFUNCS; i++)
	{
		if (str_eq(name, funcmaps[i].name))
			return &(funcmaps[i]);
	}

	return NULL;
} /* }}} */

void find_next_search_result(task *head, task *pos) /* {{{ */
{
	/* find the next search result in the list of tasks */
	task *cur;

	cur = pos;
	while (1)
	{
		/* move to next item */
		cur = cur->next;

		/* move to head if end of list is reached */
		if (cur == NULL)
		{
			cur = head;
			selline = 0;
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "search wrapped");
		}

		else
			selline++;

		/* check for match */
		if (task_match(cur, searchstring))
			return;

		/* stop if full loop was made */
		if (cur==pos)
			break;
	}

	statusbar_message(cfg.statusbar_timeout, "no matches: %s", searchstring);

	return;
} /* }}} */

var *find_var(const char *name) /* {{{ */
{
	/* attempt to find an exposed variable matching <name> */
	int i;

	for (i=0; i<NVARS; i++)
	{
		if (str_eq(name, vars[i].name))
			return &(vars[i]);
	}

	return NULL;
} /* }}} */

void handle_command(char *cmdstr) /* {{{ */
{
	/* accept a command string, determine what action to take, and execute */
	char *pos, *args = NULL;
	cmdstr = str_trim(cmdstr);

	tnc_fprintf(logfp, LOG_DEBUG, "command received: %s", cmdstr);

	/* determine command */
	pos = strchr(cmdstr, ' ');

	/* split off arguments */
	if (pos!=NULL)
	{
		(*pos) = 0;
		args = ++pos;
	}

	/* handle command & arguments */
	/* version: print version string */
	if (str_eq(cmdstr, "version"))
		statusbar_message(cfg.statusbar_timeout, "%s %s by %s\n", progname, progversion, progauthor);
	/* quit/exit: exit tasknc */
	else if (str_eq(cmdstr, "quit") || str_eq(cmdstr, "exit"))
		state.done = 1;
	/* reload: force reload of task list */
	else if (str_eq(cmdstr, "reload"))
	{
		state.reload = 1;
		statusbar_message(cfg.statusbar_timeout, "task list reloaded");
	}
	/* redraw: force redraw of screen */
	else if (str_eq(cmdstr, "redraw"))
		state.redraw = 1;
	/* show: print a variable's value */
	else if (str_eq(cmdstr, "show"))
		run_command_show(str_trim(args));
	/* set: set a variable's value */
	else if (str_eq(cmdstr, "set"))
		run_command_set(str_trim(args));
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
	/* bind: add a new keybind */
	else if (str_eq(cmdstr, "bind"))
		run_command_bind(str_trim(args));
	/* unbind: remove keybinds */
	else if (str_eq(cmdstr, "unbind"))
		run_command_unbind(str_trim(args));
	else
	{
		statusbar_message(cfg.statusbar_timeout, "error: command %s not found", cmdstr);
		tnc_fprintf(logfp, LOG_ERROR, "error: command %s not found", cmdstr);
	}

	/* debug */
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: %s", cmdstr);
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: [args] %s", args);
} /* }}} */

void handle_keypress(int c) /* {{{ */
{
	/* handle a key press on the main screen */
	keybind *this_bind;
	static const char *action_success_str[] = {
		[ACTION_COMPLETE] = "task completed",
		[ACTION_DELETE]   = "task deleted",
		[ACTION_EDIT]     = "task edited",
		[ACTION_VIEW]     = ""
	};
	static const char *action_fail_str[] = {
		[ACTION_COMPLETE] = "task complete failed",
		[ACTION_DELETE]   = "task delete failed",
		[ACTION_EDIT]     = "task edit failed",
		[ACTION_VIEW]     = ""
	};

	this_bind = keybinds;
	while (this_bind!=NULL)
	{
		if (c == this_bind->key)
		{
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "calling function @%p", this_bind->function);
			if (this_bind->function == (void *)key_scroll)
				key_scroll(this_bind->argint);
			else if (this_bind->function == (void *)key_task_action)
				key_task_action(this_bind->argint, action_success_str[this_bind->argint], action_fail_str[this_bind->argint]);
			else if (this_bind->function != NULL)
				(*(this_bind->function))(this_bind->argstr);
			break;
		}
		this_bind = this_bind->next;
	}
	if (this_bind==NULL)
		statusbar_message(cfg.statusbar_timeout, "unhandled key: %c (%d)", c, c);
} /* }}} */

void help(void) /* {{{ */
{
	/* print a list of options and program info */
	print_version();
	fprintf(stderr, "\nUsage: %s [options]\n\n", progname);
	fprintf(stderr, "  Options:\n"
			"    -l, --loglevel [value]   set log level\n"
			"    -d, --debug              debug mode\n"
			"    -f, --filter             set the initial filter\n"
			"    -h, --help               print this help message and exit\n"
			"    -v, --version            print the version and exit\n");
} /* }}} */

void key_add() /* {{{ */
{
	/* handle a keyboard direction to add new task */
	task_add();
	state.reload = 1;
	statusbar_message(cfg.statusbar_timeout, "task added");
} /* }}} */

void key_command(const char *arg) /* {{{ */
{
	/* accept and attemt to execute a command string */
	char *cmdstr;

	if (arg==NULL)
	{
		/* prepare prompt */
		statusbar_message(-1, ":");
		set_curses_mode(NCURSES_MODE_STRING);

		/* get input */
		cmdstr = calloc(cols, sizeof(char));
		wgetstr(stdscr, cmdstr);
		wipe_statusbar();

		/* reset */
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = (int)strlen(arg);
		cmdstr = calloc(arglen, sizeof(char));
		strncpy(cmdstr, arg, arglen);
	}

	/* run input command */
	if (cmdstr[0]!=0)
		handle_command(cmdstr);
	free(cmdstr);
} /* }}} */

void key_done() /* {{{ */
{
	/* wrapper function to handle keyboard instruction to quit */
	state.done = 1;
} /* }}} */

void key_filter(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to add a new filter */
	if (arg==NULL)
	{
		statusbar_message(-1, "filter by: ");
		set_curses_mode(NCURSES_MODE_STRING);
		check_free(active_filter);
		active_filter = calloc(2*cols, sizeof(char));
		wgetstr(stdscr, active_filter);
		wipe_statusbar();
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = (int)strlen(arg);
		check_free(active_filter);
		active_filter = calloc(arglen, sizeof(char));
		strncpy(active_filter, arg, arglen);
	}

	statusbar_message(cfg.statusbar_timeout, "filter applied");
	selline = 0;
	state.reload = 1;
} /* }}} */

void key_modify(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to add a new filter */
	char *argstr;

	if (arg==NULL)
	{
		statusbar_message(-1, "modify: ");
		set_curses_mode(NCURSES_MODE_STRING);
		argstr = calloc(2*cols, sizeof(char));
		wgetstr(stdscr, argstr);
		wipe_statusbar();
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = (int)strlen(arg);
		argstr = calloc(arglen+1, sizeof(char));
		strncpy(argstr, arg, arglen);
	}

	task_modify(argstr);
	free(argstr);

	statusbar_message(cfg.statusbar_timeout, "task modified");
	state.redraw = 1;
	sort_wrapper(head);
} /* }}} */

void key_reload() /* {{{ */
{
	/* wrapper function to handle keyboard instruction to reload task list */
	state.reload = 1;
	statusbar_message(cfg.statusbar_timeout, "task list reloaded");
} /* }}} */

void key_scroll(const int direction) /* {{{ */
{
	/* handle a keyboard direction to scroll */
	const char oldsel = selline;
	const char oldoffset = pageoffset;

	switch (direction)
	{
		case 'u':
			/* scroll one up */
			if (selline>0)
			{
				selline--;
				if (selline<pageoffset)
					pageoffset--;
			}
			break;
		case 'd':
			/* scroll one down */
			if (selline<taskcount-1)
			{
				selline++;
				if (selline>=pageoffset+rows-2)
					pageoffset++;
			}
			break;
		case 'h':
			/* go to first entry */
			pageoffset = 0;
			selline = 0;
			break;
		case 'e':
			/* go to last entry */
			if (taskcount>rows-2)
				pageoffset = taskcount-rows+2;
			selline = taskcount-1;
			break;
		default:
			statusbar_message(cfg.statusbar_timeout, "invalid scroll direction");
			break;
	}
	if (pageoffset!=oldoffset)
		state.redraw = 1;
	else
	{
		print_task(oldsel, NULL);
		print_task(selline, NULL);
	}
	print_title();
} /* }}} */

void key_search(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to search */
	check_free(searchstring);
	if (arg==NULL)
	{
		statusbar_message(-1, "search phrase: ");
		set_curses_mode(NCURSES_MODE_STRING);

		/* store search string  */
		searchstring = malloc((cols-16)*sizeof(char));
		wgetstr(stdscr, searchstring);
		sb_timeout = time(NULL) + 3;
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = strlen(arg);
		searchstring = calloc(arglen, sizeof(char));
		strncpy(searchstring, arg, arglen);
	}

	/* go to first result */
	find_next_search_result(head, get_task_by_position(selline));
	check_curs_pos();
	state.redraw = 1;
} /* }}} */

void key_search_next() /* {{{ */
{
	/* handle a keyboard direction to move to next search result */
	if (searchstring!=NULL)
	{
		find_next_search_result(head, get_task_by_position(selline));
		check_curs_pos();
		state.redraw = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "no active search string");
} /* }}} */

void key_sort(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to sort */
	char m;

	/* get sort mode */
	if (arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "enter sort mode: iNdex, Project, Due, pRiority");
		set_curses_mode(NCURSES_MODE_STD_BLOCKING);

		m = wgetch(stdscr);
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
		m = *arg;

	/* do sort */
	switch (m)
	{
		case 'n':
		case 'p':
		case 'd':
		case 'r':
			cfg.sortmode = m;
			sort_wrapper(head);
			break;
		case 'N':
		case 'P':
		case 'D':
		case 'R':
			cfg.sortmode = m+32;
			sort_wrapper(head);
			break;
		default:
			statusbar_message(cfg.statusbar_timeout, "invalid sort mode");
			break;
	}
	state.redraw = 1;
} /* }}} */

void key_sync() /* {{{ */
{
	/* handle a keyboard direction to sync */
	int ret;

	if (cfg.silent_shell == '1')
	{
		statusbar_message(cfg.statusbar_timeout, "synchronizing tasks...");
		ret = system("yes n | task merge 2&> /dev/null");
		if (ret==0)
			ret = system("task push 2&> /dev/null");
	}
	else
	{
		def_prog_mode();
		endwin();
		ret = system("yes n | task merge");
		if (ret==0)
			ret = system("task push");
	}
	wrefresh(stdscr);
	if (ret==0)
	{
		statusbar_message(cfg.statusbar_timeout, "tasks synchronized");
		state.reload = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "task syncronization failed");
} /* }}} */

void key_task_action(const task_action_type action, const char *msg_success, const char *msg_fail) /* {{{ */
{
	/* handle a keyboard direction to run a task command */
	int ret;

	def_prog_mode();
	endwin();
	if (action!=ACTION_VIEW && action!=ACTION_COMPLETE && action!=ACTION_DELETE)
		state.reload = 1;
	ret = task_action(action);
	wrefresh(stdscr);
	if (ret==0)
		statusbar_message(cfg.statusbar_timeout, msg_success);
	else
		statusbar_message(cfg.statusbar_timeout, msg_fail);
} /* }}} */

void key_undo() /* {{{ */
{
	/* handle a keyboard direction to run an undo */
	int ret;

	if (cfg.silent_shell=='1')
	{
		statusbar_message(cfg.statusbar_timeout, "running task undo...");
		ret = system("task undo > /dev/null");
	}
	else
	{
		def_prog_mode();
		endwin();
		ret = system("task undo");
	}
	wrefresh(stdscr);
	if (ret==0)
	{
		statusbar_message(cfg.statusbar_timeout, "undo executed");
		state.reload = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "undo execution failed (%d)", ret);
} /* }}} */

bool match_string(const char *haystack, const char *needle) /* {{{ */
{
	/* match a string to a regex */
	regex_t regex;
	char ret;

	/* check for NULL haystack or needle */
	if (haystack==NULL || needle==NULL)
		return 0;

	/* compile and run regex */
	if (regcomp(&regex, needle, REGEX_OPTS) != 0)
		return 0;
	if (regexec(&regex, haystack, 0, 0, 0) != REG_NOMATCH)
		ret = 1;
	else
		ret = 0;
	regfree(&regex);
	return ret;
} /* }}} */

char max_project_length() /* {{{ */
{
	char len = 0;
	task *cur;

	cur = head;
	while (cur!=NULL)
	{
		if (cur->project!=NULL)
		{
			char l = strlen(cur->project);
			if (l>len)
				len = l;
		}
		cur = cur->next;
	}

	return len;
} /* }}} */

void nc_colors(void) /* {{{ */
{
	if (has_colors())
	{
		start_color();
		use_default_colors();
		init_pair(1, COLOR_BLUE,        COLOR_BLACK);   /* title bar */
		init_pair(2, COLOR_WHITE,       -1);            /* default task */
		init_pair(3, COLOR_CYAN,        -1);            /* selected task */
		init_pair(8, COLOR_RED,         -1);            /* error message */
	}
} /* }}} */

void nc_end(int sig) /* {{{ */
{
	/* terminate ncurses */
	delwin(stdscr);
	endwin();

	switch (sig)
	{
		case SIGINT:
			tnc_fprintf(stdout, LOG_DEFAULT, "aborted");
			tnc_fprintf(logfp, LOG_DEBUG, "received SIGINT, exiting");
			break;
		case SIGSEGV:
			tnc_fprintf(logfp, LOG_WARN, "SEGFAULT");
			tnc_fprintf(logfp, LOG_WARN, "segmentation fault, exiting");
			break;
		case SIGKILL:
			tnc_fprintf(stdout, LOG_WARN, "killed");
			tnc_fprintf(logfp, LOG_WARN, "received SIGKILL, exiting");
			break;
		default:
			tnc_fprintf(stdout, LOG_DEFAULT, "done");
			tnc_fprintf(logfp, LOG_DEBUG, "exiting with code %d", sig);
			break;
	}

	cleanup();

	exit(0);
} /* }}} */

void nc_main() /* {{{ */
{
	/* ncurses main function */
	WINDOW *stdscr;
	int c, oldrows, oldcols;
	fieldlengths.project = max_project_length();
	fieldlengths.date = DATELENGTH;

	/* initialize ncurses */
	tnc_fprintf(stdout, LOG_DEBUG, "starting ncurses...");
	signal(SIGINT, nc_end);
	signal(SIGKILL, nc_end);
	signal(SIGSEGV, nc_end);
	if ((stdscr = initscr()) == NULL ) {
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
	}

	/* set curses settings */
	set_curses_mode(NCURSES_MODE_STD);

	/* print main screen */
	check_screen_size();
	getmaxyx(stdscr, oldrows, oldcols);
	fieldlengths.description = oldcols-fieldlengths.project-1-fieldlengths.date;
	task_count();
	print_title();
	attrset(COLOR_PAIR(0));
	print_task_list();
	wrefresh(stdscr);

	/* main loop */
	while (1)
	{
		/* set variables for determining actions */
		state.done = 0;
		state.redraw = 0;
		state.reload = 0;

		/* get the screen size */
		getmaxyx(stdscr, rows, cols);

		/* check for a screen thats too small */
		check_screen_size();

		/* check for size changes */
		if (cols!=oldcols || rows!=oldrows)
		{
			state.redraw = 1;
			wipe_statusbar();
		}
		oldcols = cols;
		oldrows = rows;

		/* get a character */
		c = wgetch(stdscr);

		/* handle the character */
		handle_keypress(c);

		if (state.done==1)
			break;
		if (state.reload==1)
		{
			wipe_tasklist();
			reload_tasks();
			task_count();
			state.redraw = 1;
		}
		if (state.redraw==1)
		{
			fieldlengths.project = max_project_length();
			fieldlengths.description = cols-fieldlengths.project-1-fieldlengths.date;
			print_title();
			print_task_list();
			check_curs_pos();
			wrefresh(stdscr);
		}
		if (sb_timeout>0 && sb_timeout<time(NULL))
		{
			sb_timeout = 0;
			wipe_statusbar();
		}
	}
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

void print_task(int tasknum, task *this) /* {{{ */
{
	/* print a task specified by number */
	bool sel = 0;
	char *tmp;
	int x, y;

	/* determine position to print */
	y = tasknum-pageoffset+1;
	if (y<=0 || y>=rows-1)
		return;

	/* find task pointer if necessary */
	if (this==NULL)
		this = get_task_by_position(tasknum);

	/* check if this is NULL */
	if (this==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "task %d is null", tasknum);
		return;
	}

	/* determine if line is selected */
	if (tasknum==selline)
		sel = 1;

	/* wipe line */
	attrset(COLOR_PAIR(0));
	for (x=0; x<cols; x++)
		mvaddch(y, x, ' ');

	/* evaluate line */
	attrset(COLOR_PAIR(2+sel));
	tmp = (char *)eval_string(2*cols, formats.task, this, NULL, 0);
	umvaddstr_align(stdscr, y, tmp);
	free(tmp);
} /* }}} */

void print_task_list() /* {{{ */
{
	/* print every task in the task list */
	task *cur;
	short counter = 0;

	cur = head;
	while (cur!=NULL)
	{
		print_task(counter, cur);

		/* move to next item */
		counter++;
		cur = cur->next;
	}
	if (counter<cols-2)
		wipe_screen(counter+1-pageoffset, cols-1);
} /* }}} */

void print_title() /* {{{ */
{
	/* print the window title bar */
	char *tmp0;
	int x;

	/* wipe bar and print bg color */
	attrset(COLOR_PAIR(1));
	for (x=0; x<cols; x++)
		mvaddch(0, x, ' ');

	/* evaluate title string */
	tmp0 = (char *)eval_string(2*cols, formats.title, NULL, NULL, 0);
	umvaddstr_align(stdscr, 0, tmp0);
	free(tmp0);
} /* }}} */

void print_version(void) /* {{{ */
{
	/* print info about the currently running program */
	printf("%s %s by %s\n", progname, progversion, progauthor);
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

void run_command_bind(char *args) /* {{{ */
{
	/* create a new keybind */
	int key;
	char *function, *arg, *keystr, *aarg;
	void (*func)();
	funcmap *fmap;

	/* make sure an argument was passed */
	if (args==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: key must be specified (%s)", args);
		return;
	}

	/* split off key */
	keystr = args;
	function = strchr(keystr, ' ');
	if (function==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "bind: function must be specified (%s)", args);
		return;
	}
	(*function) = 0;
	function++;

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
	fmap = find_function(function);
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
	{
		aarg = calloc((int)strlen(arg), sizeof(char));
		strcpy(aarg, arg);
	}
	else
		aarg = NULL;

	/* add keybind */
	add_keybind(key, func, aarg);
	statusbar_message(cfg.statusbar_timeout, "key bound");
} /* }}} */

void run_command_unbind(char *keystr) /* {{{ */
{
	/* handle a keyboard instruction to unbind a key */
	const int key = parse_key(keystr);

	remove_keybinds(key);
	statusbar_message(cfg.statusbar_timeout, "key unbound: %c (%d)", key, key);
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
			ret = NULL==strcpy(*(char **)(this_var->ptr), value);
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

void set_curses_mode(const ncurses_mode mode) /* {{{ */
{
	/* set curses settings for various common modes */
	switch (mode)
	{
		case NCURSES_MODE_STD:
			keypad(stdscr, TRUE);   /* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			nc_colors();            /* initialize colors */
			curs_set(0);            /* set cursor invisible */
			timeout(cfg.nc_timeout);/* timeout getch */
			break;
		case NCURSES_MODE_STD_BLOCKING:
			keypad(stdscr, TRUE);   /* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			nc_colors();            /* initialize colors */
			curs_set(0);            /* set cursor invisible */
			timeout(-1);            /* no timeout on getch */
			break;
		case NCURSES_MODE_STRING:
			curs_set(2);            /* set cursor visible */
			nocbreak();             /* wait for \n */
			echo();                 /* echo input */
			timeout(-1);            /* no timeout on getch */
			break;
		default:
			break;
	}
} /* }}} */

void statusbar_message(const int dtmout, const char *format, ...) /* {{{ */
{
	/* print a message in the statusbar */
	va_list args;
	char *message;

	/* format message */
	va_start(args, format);
	vasprintf(&message, format, args);
	va_end(args);

	/* check for active screen */
	if (stdscr==NULL)
	{
		tnc_fprintf(stdout, LOG_INFO, message);
		tnc_fprintf(logfp, LOG_DEBUG, "(stdscr==NULL) %s", message);
		free(message);
		return;
	}

	wipe_statusbar();

	/* print message */
	umvaddstr(stdscr, rows-1, 0, message);
	free(message);

	/* set timeout */
	if (dtmout>=0)
		sb_timeout = time(NULL) + dtmout;

	wrefresh(stdscr);
} /* }}} */

char *str_trim(char *str) /* {{{ */
{
	/* remove trailing and leading spaces from a string in place
	 * returns the pointer to the new start */
	char *pos;

	/* leading */
	while ((*str)==' ')
		str++;

	/* trailing */
	pos = str;
	while ((*pos)!=0)
		pos++;
	while (*(--pos)==' ')
		(*pos) = 0;

	return str;
} /* }}} */

int task_action(const task_action_type action) /* {{{ */
{
	/* spawn a command to perform an action on a task */
	task *cur;
	static const char *str_for_action[] = {
		[ACTION_EDIT]     = "edit",
		[ACTION_COMPLETE] = "done",
		[ACTION_DELETE]   = "del",
		[ACTION_VIEW]     = "info"
	};
	const char *actionstr = str_for_action[(int)action];
	char *cmd, *redir;
	const bool wait = action == ACTION_VIEW;
	int ret;

	/* move to correct task */
	cur = get_task_by_position(selline);

	/* determine whether stdio should be used */
	if (cfg.silent_shell && action!=ACTION_VIEW && action!=ACTION_EDIT)
	{
		statusbar_message(cfg.statusbar_timeout, "running task %s", actionstr);
		redir = "> /dev/null";
	}
	else
		redir = "";

	/* generate and run command */
	cmd = malloc(128*sizeof(char));

	/* update task index if version<2*/
	if (cfg.version[0]<'2')
	{
		cur->index = get_task_id(cur->uuid);
		if (cur->index==0)
			return -1;
		sprintf(cmd, "task %s %d %s", actionstr, cur->index, redir);
	}

	/* if version is >=2, use uuid index */
	else
		sprintf(cmd, "task %s %s %s", cur->uuid, actionstr, redir);

	tnc_fprintf(logfp, LOG_DEBUG, "running: %s", cmd);
	ret = system(cmd);
	free(cmd);
	if (wait)
	{
		puts("press ENTER to return");
		fflush(stdout);
		getchar();
	}

	/* remove from task list if command was successful */
	if (ret==0 && (action==ACTION_DELETE || action==ACTION_COMPLETE))
	{
		if (cur==head)
			head = cur->next;
		else
			cur->prev->next = cur->next;
		if (cur->next!=NULL)
			cur->next->prev = cur->prev;
		free_task(cur);
		taskcount--;
		totaltaskcount--;
		state.redraw = 1;
	}

	return ret;
} /* }}} */

void task_add(void) /* {{{ */
{
	/* create a new task by adding a generic task
	 * then letting the user edit it
	 */
	FILE *cmdout;
	char *cmd, line[TOTALLENGTH], *redir;
	const char addstr[] = "Created task ";
	unsigned short tasknum;

	/* determine whether stdio should be used */
	if (cfg.silent_shell)
	{
		statusbar_message(cfg.statusbar_timeout, "running task add");
		redir = "> /dev/null";
	}
	else
		redir = "";

	/* add new task */
	cmd = malloc(128*sizeof(char));
	sprintf(cmd, "task add new task %s", redir);
	tnc_fprintf(logfp, LOG_DEBUG, "running: %s", cmd);
	cmdout = popen("task add new task", "r");
	while (fgets(line, sizeof(line)-1, cmdout) != NULL)
	{
		if (strncmp(line, addstr, strlen(addstr))==0)
			if (sscanf(line, "Created task %hu.", &tasknum))
				break;
	}
	pclose(cmdout);
	free(cmd);

	/* edit task */
	def_prog_mode();
	cmd = malloc(128*sizeof(char));
	if (cfg.version[0]<'2')
		sprintf(cmd, "task edit %d", tasknum);
	else
		sprintf(cmd, "task %d edit", tasknum);
	endwin();
	system(cmd);
	free(cmd);
	reset_prog_mode();
	wrefresh(stdscr);
} /* }}} */

void task_count() /* {{{ */
{
	taskcount = 0;
	totaltaskcount = 0;
	task *cur;

	cur = head;
	while (cur!=NULL)
	{
		taskcount++;
		totaltaskcount++;
		cur = cur->next;
	}
} /* }}} */

void task_modify(const char *argstr) /* {{{ */
{
	/* run a modify command on the selected task */
	task *cur;
	char *cmd;
	FILE *run;
	int arglen;

	if (argstr!=NULL)
		arglen = strlen(argstr);
	else
		arglen = 0;

	cur = get_task_by_position(selline);
	cmd = calloc(64+arglen, sizeof(char));

	sprintf(cmd, "task %s modify ", cur->uuid);
	if (arglen>0)
		strcat(cmd, argstr);

	run = popen(cmd, "r");
	pclose(run);

	reload_task(cur);

	free(cmd);
} /* }}} */

static bool task_match(const task *cur, const char *str) /* {{{ */
{
	if (match_string(cur->project, str) ||
			match_string(cur->description, str) ||
			match_string(cur->tags, str))
		return 1;
	else
		return 0;
} /* }}} */

int umvaddstr(WINDOW *win, const int y, const int x, const char *format, ...) /* {{{ */
{
	/* convert a string to a wchar string and mvaddwstr */
	int len, r;
	wchar_t *wstr;
	char *str;
	va_list args;

	/* build str */
	va_start(args, format);
	const int slen = sizeof(wchar_t)*(cols-x+1)/sizeof(char);
	str = calloc(slen, sizeof(char));
	vsnprintf(str, slen-1, format, args);
	va_end(args);

	/* allocate wchar_t string */
	len = strlen(str)+1;
	wstr = calloc(len, sizeof(wchar_t));

	/* check for valid allocation */
	if (wstr==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "critical: umvaddstr failed to malloc");
		return -1;
	}

	/* perform conversion and write to screen */
	mbstowcs(wstr, str, len);
	len = wcslen(wstr);
	if (len>cols-x)
		len = cols-x;
	r = mvwaddnwstr(win, y, x, wstr, len);

	/* free memory allocated */
	free(wstr);
	free(str);

	return r;
} /* }}} */

int umvaddstr_align(WINDOW *win, const int y, char *str) /* {{{ */
{
	/* evaluate an aligned string */
	char *right, *pos;
	int ret, tmp;

	/* find break */
	pos = strstr(str, "$>");

	/* end left string */
	(*pos) = 0;

	/* start right string */
	right = (pos+2);

	/* print strings */
	tmp = umvaddstr(win, y, 0, str);
	ret = umvaddstr(win, y, cols-strlen(right), right);
	if (tmp>ret)
		ret = tmp;

	/* fix string */
	(*pos) = '$';

	return ret;
} /* }}} */

char *utc_date(const unsigned int timeint) /* {{{ */
{
	/* convert a utc time uint to a string */
	struct tm tmr, *now;
	time_t cur;
	char *timestr, *srcstr;

	/* convert the input timeint to a string */
	srcstr = malloc(16*sizeof(char));
	sprintf(srcstr, "%d", timeint);

	/* extract time struct from string */
	strptime(srcstr, "%s", &tmr);
	free(srcstr);

	/* get current time */
	time(&cur);
	now = localtime(&cur);

	/* set time to now if 0 was the argument */
	if (timeint==0)
		tmr = *now;

	/* convert thte time to a formatted string */
	timestr = malloc(TIMELENGTH*sizeof(char));
	if (now->tm_year != tmr.tm_year)
		strftime(timestr, TIMELENGTH, "%F", &tmr);
	else
		strftime(timestr, TIMELENGTH, "%b %d", &tmr);

	return timestr;
} /* }}} */

char *var_value_message(var *v, bool printname) /* {{{ */
{
	/* format a message containing the name and value of a variable */
	char *message;
	char *value;

	switch(v->type)
	{
		case VAR_INT:
			if (str_eq(v->name, "selected_line"))
				asprintf(&value, "%d", selline+1);
			else
				asprintf(&value, "%d", *(int *)(v->ptr));
			break;
		case VAR_CHAR:
			asprintf(&value, "%c", *(char *)(v->ptr));
			break;
		case VAR_STR:
			asprintf(&value, "%s", *(char **)(v->ptr));
			break;
		default:
			asprintf(&value, "variable type unhandled");
			break;
	}

	if (printname == 0)
		return value;

	message = malloc(strlen(v->name) + strlen(value) + 3);
	strcpy(message, v->name);
	strcat(message, ": ");
	strcat(message, value);
	free(value);

	return message;
} /* }}} */

void wipe_screen(const short startl, const short stopl) /* {{{ */
{
	/* clear specified lines of the screen */
	int y, x;

	attrset(COLOR_PAIR(0));

	for (y=startl; y<=stopl; y++)
		for (x=0; x<cols; x++)
			mvaddch(y, x, ' ');
} /* }}} */

int main(int argc, char **argv) /* {{{ */
{
	/* declare variables */
	int c, debug = 0;

	/* open log */
	logfp = fopen(LOGFILE, "a");
	tnc_fprintf(logfp, LOG_DEBUG, "%s started", progname);

	/* set defaults */
	cfg.loglvl = LOGLVL_DEFAULT;
	setlocale(LC_ALL, "");

	/* handle arguments */
	static struct option long_options[] =
	{
		{"help",     no_argument,       0, 'h'},
		{"debug",    no_argument,       0, 'd'},
		{"version",  no_argument,       0, 'v'},
		{"loglevel", required_argument, 0, 'l'},
		{"filter",   required_argument, 0, 'f'},
		{0, 0, 0, 0}
	};
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "l:hvdf:", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'l':
				cfg.loglvl = (char) atoi(optarg);
				printf("loglevel: %d\n", (int)cfg.loglvl);
				break;
			case 'v':
				print_version();
				return 0;
				break;
			case 'd':
				debug = 1;
				break;
			case 'f':
				active_filter = malloc(strlen(optarg)*sizeof(char));
				strcpy(active_filter, optarg);
				break;
			case 'h':
			case '?':
				help();
				return 0;
				break;
			default:
				return 1;
		}
	}

	/* read config file */
	configure();

	/* build task list */
	head = get_tasks(NULL);
	if (head==NULL)
	{
		tnc_fprintf(stdout, LOG_WARN, "it appears that your task list is empty");
		tnc_fprintf(stdout, LOG_WARN, "please add some tasks for %s to manage\n", progname);
		return 1;
	}

	/* run ncurses */
	if (!debug)
	{
		tnc_fprintf(logfp, LOG_DEBUG, "running gui");
		nc_main();
		nc_end(0);
	}

	/* debug mode */
	else
	{
		task_count();
		printf("task count: %d\n", totaltaskcount);
		char *test;
		asprintf(&test, "set task_version 2.1");
		char *tmp = var_value_message(find_var("task_version"), 1);
		puts(tmp);
		free(tmp);
		handle_command(test);
		tmp = var_value_message(find_var("task_version"), 1);
		puts(tmp);
		free(tmp);
		free(test);
		asprintf(&tmp, "set search_string tasknc");
		handle_command(tmp);
		test = var_value_message(find_var("search_string"), 1);
		puts(test);
		free(test);
		printf("selline: %d\n", selline);
		find_next_search_result(head, head);
		printf("selline: %d\n", selline);
		task *t = get_task_by_position(selline);
		if (t==NULL)
			puts("???");
		else
			puts(t->description);
		free(tmp);
		asprintf(&tmp, "  test string  ");
		printf("%s (%d)\n", tmp, (int)strlen(tmp));
		test = str_trim(tmp);
		printf("%s (%d)\n", test, (int)strlen(test));
		free(tmp);

		/* evaluating a format string */
		const char *titlefmt = "$program_name;();$program_version; $filter_string//$task_count\\/$badvar";
		printf("%s\n", eval_string(100, titlefmt, NULL, NULL, 0));

		cleanup();
	}

	/* done */
	tnc_fprintf(logfp, LOG_DEBUG, "exiting");
	return 0;
} /* }}} */
