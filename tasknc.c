/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <curses.h>
#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "color.h"
#include "command.h"
#include "common.h"
#include "config.h"
#include "tasknc.h"
#include "tasklist.h"
#include "tasks.h"
#include "log.h"
#include "keys.h"
#include "pager.h"
#include "test.h"

/* global variables {{{ */
const char *progname = PROGNAME;
const char *progauthor = PROGAUTHOR;
const char *progversion = PROGVERSION;

config cfg;                             /* runtime config struct */
short pageoffset = 0;                   /* number of tasks page is offset */
time_t sb_timeout = 0;                  /* when statusbar should be cleared */
char *searchstring = NULL;              /* currently active search string */
int selline = 0;                        /* selected line number */
int rows, cols;                         /* size of the ncurses window */
int taskcount;                          /* number of tasks */
char *active_filter = NULL;             /* a string containing the active filter string */
task *head = NULL;                      /* the current top of the list */
FILE *logfp;                            /* handle for log file */
keybind *keybinds = NULL;

/* runtime status */
bool redraw;
bool reload;
bool done;

/* windows */
WINDOW *header = NULL;
WINDOW *tasklist = NULL;
WINDOW *statusbar = NULL;
WINDOW *pager = NULL;
/* }}} */

/* user-exposed variables & functions {{{ */
var vars[] = {
	{"curs_timeout",      VAR_INT,  &(cfg.nc_timeout)},
	{"filter_string",     VAR_STR,  &active_filter},
	{"follow_task",       VAR_INT,  &(cfg.follow_task)},
	{"log_level",         VAR_INT,  &(cfg.loglvl)},
	{"program_author",    VAR_STR,  &progauthor},
	{"program_name",      VAR_STR,  &progname},
	{"program_version",   VAR_STR,  &progversion},
	{"search_string",     VAR_STR,  &searchstring},
	{"selected_line",     VAR_INT,  &selline},
	{"sort_mode",         VAR_STR,  &(cfg.sortmode)},
	{"statusbar_timeout", VAR_INT,  &(cfg.statusbar_timeout)},
	{"task_count",        VAR_INT,  &taskcount},
	{"task_format",       VAR_STR,  &(cfg.formats.task)},
	{"task_version",      VAR_STR,  &(cfg.version)},
	{"title_format",      VAR_STR,  &(cfg.formats.title)},
	{"view_format",       VAR_STR,  &(cfg.formats.view)},
};

funcmap funcmaps[] = {
	{"add",         (void *)key_tasklist_add,             0, MODE_TASKLIST},
	{"bind",        (void *)run_command_bind,             1, MODE_ANY},
	{"color",       (void *)run_command_color,            1, MODE_ANY},
	{"command",     (void *)key_command,                  0, MODE_ANY},
	{"complete",    (void *)key_tasklist_complete,        0, MODE_ANY},
	{"delete",      (void *)key_tasklist_delete,          0, MODE_ANY},
	{"edit",        (void *)key_tasklist_edit,            0, MODE_ANY},
	{"filter",      (void *)key_tasklist_filter,          0, MODE_TASKLIST},
	{"f_redraw",    (void *)force_redraw,                 0, MODE_ANY},
	{"help",        (void *)help_window,                  0, MODE_ANY},
	{"modify",      (void *)key_tasklist_modify,          0, MODE_TASKLIST},
	{"quit",        (void *)key_done,                     0, MODE_TASKLIST},
	{"quit",        (void *)key_pager_close,              0, MODE_PAGER},
	{"reload",      (void *)key_tasklist_reload,          0, MODE_TASKLIST},
	{"scroll_down", (void *)key_tasklist_scroll_down,     0, MODE_TASKLIST},
	{"scroll_down", (void *)key_pager_scroll_down,        0, MODE_PAGER},
	{"scroll_end",  (void *)key_tasklist_scroll_end,      0, MODE_TASKLIST},
	{"scroll_end",  (void *)key_pager_scroll_end,         0, MODE_PAGER},
	{"scroll_home", (void *)key_tasklist_scroll_home,     0, MODE_TASKLIST},
	{"scroll_home", (void *)key_pager_scroll_home,        0, MODE_PAGER},
	{"scroll_up",   (void *)key_tasklist_scroll_up,       0, MODE_TASKLIST},
	{"scroll_up",   (void *)key_pager_scroll_up,          0, MODE_PAGER},
	{"search",      (void *)key_tasklist_search,          0, MODE_TASKLIST},
	{"search_next", (void *)key_tasklist_search_next,     0, MODE_TASKLIST},
	{"set",         (void *)run_command_set,              1, MODE_ANY},
	{"shell",       (void *)key_task_interactive_command, 1, MODE_ANY},
	{"shell_bg",    (void *)key_task_background_command,  1, MODE_ANY},
	{"show",        (void *)run_command_show,             1, MODE_ANY},
	{"sort",        (void *)key_tasklist_sort,            0, MODE_TASKLIST},
	{"source",      (void *)run_command_source,           1, MODE_ANY},
	{"stats",       (void *)view_stats,                   0, MODE_ANY},
	{"sync",        (void *)key_tasklist_sync,            0, MODE_TASKLIST},
	{"unbind",      (void *)run_command_unbind,           1, MODE_ANY},
	{"undo",        (void *)key_tasklist_undo,            0, MODE_TASKLIST},
	{"view",        (void *)key_tasklist_view,            0, MODE_TASKLIST},
};
/* }}} */

void check_resize() /* {{{ */
{
	/* check for a screen resize and handle it */
	static int oldheight = 0;
	static int oldwidth = 0;
	int height, width;

	/* first run */
	if (oldheight == 0)
		oldheight = getmaxy(stdscr);
	if (oldwidth == 0)
		oldwidth  = getmaxx(stdscr);

	/* get current dimensions */
	height = getmaxy(stdscr);
	width  = getmaxx(stdscr);

	/* check for resize */
	if (oldheight != height || oldwidth != width)
		handle_resize();

	/* save position */
	oldheight = height;
	oldwidth = width;
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
			wattrset(stdscr, get_colors(OBJECT_ERROR, NULL, NULL));
			mvaddstr(0, 0, "screen dimensions too small");
			wrefresh(stdscr);
			wattrset(stdscr, COLOR_PAIR(0));
			usleep(100000);
		}
		count++;
		rows = LINES;
		cols = COLS;
	} while (cols<DATELENGTH+20+cfg.fieldlengths.project || rows<5);
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
	free(cfg.formats.task);
	free(cfg.formats.title);
	free(cfg.formats.view);
	free(active_filter);
	while (keybinds!=NULL)
	{
		lastbind = keybinds;
		keybinds = keybinds->next;
		check_free(lastbind->argstr);
		free(lastbind);
	}
	free_colors();

	/* close open files */
	fflush(logfp);
	fclose(logfp);
} /* }}} */

void configure(void) /* {{{ */
{
	/* parse config file to get runtime options */
	FILE *cmd;
	char *filepath, *xdg_config_home, *home;
	int ret = 0;

	/* set default settings */
	cfg.nc_timeout = NCURSES_WAIT;                          /* time getch will wait */
	cfg.statusbar_timeout = STATUSBAR_TIMEOUT_DEFAULT;      /* default time before resetting statusbar */
	cfg.sortmode = strdup("drpu");                          /* determine sort order */
	cfg.follow_task = true;                                 /* follow task after it is moved */

	/* set default formats */
	cfg.formats.title = strdup(" $program_name ($selected_line/$task_count) $> $date");
	cfg.formats.task = strdup(" $project $description $> ?$due?$due?$-6priority?");
	cfg.formats.view = strdup(" task info");

	/* set initial filter */
	active_filter = strdup("status:pending");

	/* get task version */
	cmd = popen("task --version", "r");
	while (ret != 1)
		ret = fscanf(cmd, "%m[0-9.-] ", &(cfg.version));
	tnc_fprintf(logfp, LOG_DEBUG, "task version: %s", cfg.version);
	pclose(cmd);

	/* default keybinds */
	add_keybind(ERR,           NULL,                     NULL,            MODE_TASKLIST);
	add_keybind(ERR,           NULL,                     NULL,            MODE_PAGER);
	add_keybind(KEY_RESIZE,    handle_resize,            NULL,            MODE_ANY);
	add_keybind('k',           key_tasklist_scroll_up,   NULL,            MODE_TASKLIST);
	add_keybind('k',           key_pager_scroll_up,      NULL,            MODE_PAGER);
	add_keybind(KEY_UP,        key_tasklist_scroll_up,   NULL,            MODE_TASKLIST);
	add_keybind(KEY_UP,        key_pager_scroll_up,      NULL,            MODE_PAGER);
	add_keybind('j',           key_tasklist_scroll_down, NULL,            MODE_TASKLIST);
	add_keybind('j',           key_pager_scroll_down,    NULL,            MODE_PAGER);
	add_keybind(KEY_DOWN,      key_tasklist_scroll_down, NULL,            MODE_TASKLIST);
	add_keybind(KEY_DOWN,      key_pager_scroll_down,    NULL,            MODE_PAGER);
	add_keybind('g',           key_tasklist_scroll_home, NULL,            MODE_TASKLIST);
	add_keybind(KEY_HOME,      key_tasklist_scroll_home, NULL,            MODE_TASKLIST);
	add_keybind('g',           key_pager_scroll_home,    NULL,            MODE_PAGER);
	add_keybind(KEY_HOME,      key_pager_scroll_home,    NULL,            MODE_PAGER);
	add_keybind('G',           key_pager_scroll_end,     NULL,            MODE_PAGER);
	add_keybind(KEY_END,       key_pager_scroll_end,     NULL,            MODE_PAGER);
	add_keybind('G',           key_tasklist_scroll_end,  NULL,            MODE_TASKLIST);
	add_keybind(KEY_END,       key_tasklist_scroll_end,  NULL,            MODE_TASKLIST);
	add_keybind('e',           key_tasklist_edit,        NULL,            MODE_ANY);
	add_keybind('r',           key_tasklist_reload,      NULL,            MODE_TASKLIST);
	add_keybind('u',           key_tasklist_undo,        NULL,            MODE_TASKLIST);
	add_keybind('d',           key_tasklist_delete,      NULL,            MODE_ANY);
	add_keybind('c',           key_tasklist_complete,    NULL,            MODE_ANY);
	add_keybind('a',           key_tasklist_add,         NULL,            MODE_TASKLIST);
	add_keybind('v',           key_tasklist_view,        NULL,            MODE_TASKLIST);
	add_keybind(13,            key_tasklist_view,        NULL,            MODE_TASKLIST);
	add_keybind(KEY_ENTER,     key_tasklist_view,        NULL,            MODE_TASKLIST);
	add_keybind('s',           key_tasklist_sort,        NULL,            MODE_TASKLIST);
	add_keybind('/',           key_tasklist_search,      NULL,            MODE_TASKLIST);
	add_keybind('n',           key_tasklist_search_next, NULL,            MODE_TASKLIST);
	add_keybind('f',           key_tasklist_filter,      NULL,            MODE_TASKLIST);
	add_keybind('y',           key_tasklist_sync,        NULL,            MODE_TASKLIST);
	add_keybind('q',           key_done,                 NULL,            MODE_TASKLIST);
	add_keybind('q',           key_pager_close,          NULL,            MODE_PAGER);
	add_keybind(';',           key_command,              NULL,            MODE_TASKLIST);
	add_keybind(':',           key_command,              NULL,            MODE_ANY);
	add_keybind('h',           help_window,              NULL,            MODE_ANY);
	add_keybind(12,            force_redraw,             NULL,            MODE_TASKLIST);
	add_keybind(12,            force_redraw,             NULL,            MODE_PAGER);

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
	run_command_source(filepath);
	free(filepath);
} /* }}} */

const char *eval_string(const int maxlen, char *fmt, const task *this, char *str, int position) /* {{{ */
{
	/* evaluate a string with format variables */
	int i, condoffset;
	char *field = NULL, *var = NULL, *orig_fmt, *cond[4], *condseg, *condstr;
	int fieldlen = -1, fieldwidth = -1, varlen = -1, condlen;
	bool free_field = false, condition, right_align = false;

	/* check if string is done */
	if (*fmt == 0)
		return str;

	/* allocate string if necessary */
	if (str==NULL)
		str = calloc(maxlen+1, sizeof(char));

	/* save original format in case fallback is needed */
	orig_fmt = fmt;

	/* check for a conditional */
	if (*fmt == '?')
	{
		/* get positions of each section */
		cond[0] = fmt;
		for (i=1; i<4; i++)
		{
			fmt = strchr(fmt+1, '?');
			cond[i] = fmt;
			if (cond[i] == NULL)
			{
				tnc_fprintf(logfp, LOG_ERROR, "malformed conditional in format string: %s (%d)", fmt, i);
				return "malformed conditional";
			}
		}
		fmt = orig_fmt+1;

		/* determine whether conditional is true */
		condlen = cond[1]-cond[0]-1;
		condstr = calloc(1+condlen, sizeof(char));
		strncpy(condstr, fmt, condlen);
		condseg = (char *)eval_string(3*cols, condstr, this, NULL, 0);
		condition = !(condseg == NULL || *condseg == ' ' || str_starts_with(condseg, "(null)"));
		free(condstr);
		free(condseg);

		/* evaluate proper segment of string */
		if (condition)
			condoffset = 1;
		else
			condoffset = 2;
		condlen = cond[1+condoffset]-cond[condoffset]-1;
		condstr = calloc(1+condlen, sizeof(char));
		strncpy(condstr, cond[condoffset]+1, condlen);
		condseg = (char *)eval_string(3*cols, condstr, this, NULL, 0);
		field = condseg;
		free_field = 1;
		free(condstr);
		varlen = cond[3]-cond[0];

		/* copy field */
		fieldwidth = strlen(field);

		/* move along */
		goto done;
	}

	/* check if this is a variable */
	if (strchr("$?", *fmt) == NULL)
	{
		field = calloc(2, sizeof(char));
		*field = *fmt;
		free_field = true;
		fieldlen = 1;
		fieldwidth = 1;
		varlen = 1;
		goto done;
	}
	fmt++;

	/* check for alignment marker */
	if (*fmt == '-')
	{
		right_align = true;
		fmt++;
	}

	/* get field width if specified */
	while (*fmt <= '9' && *fmt >= '0')
	{
		if (fieldwidth == -1)
			fieldwidth = (*fmt)-48;
		else
		{
			fieldwidth *= 10;
			fieldwidth += (*fmt)-48;
		}
		fmt++;
	}

	/* try for a special format string */
	if (str_starts_with(fmt, "date"))
	{
		field = utc_date(0);
		fieldlen = strlen(field);
		if (fieldwidth == -1)
			fieldwidth = fieldlen;
		free_field = true;
		var = "date";
		goto done;
	}

	/* try for a variable relative to the current task */
	else if (this!=NULL)
	{
		if (str_starts_with(fmt, "due"))
		{
			if (this->due)
				field = utc_date(this->due);
			else
			{
				field = calloc(2, sizeof(char));
				*field = ' ';
			}
			fieldlen = strlen(field);
			free_field = true;
			if (fieldwidth == -1)
				fieldwidth = fieldlen;
			var = "due";
		}
		else if (str_starts_with(fmt, "priority"))
		{
			field = calloc(2, sizeof(char));
			*field = this->priority;
			free_field = true;
			fieldlen = 1;
			if (fieldwidth == -1)
				fieldwidth = 1;
			var = "priority";
		}
		else if (str_starts_with(fmt, "description"))
		{
			field = this->description;
			if (fieldwidth == -1)
				fieldwidth = cfg.fieldlengths.description;
			var = "description";
		}
		else if (str_starts_with(fmt, "project"))
		{
			field = this->project;
			if (fieldwidth == -1)
				fieldwidth = cfg.fieldlengths.project;
			var = "project";
		}
		else if (str_starts_with(fmt, "uuid"))
		{
			field = this->uuid;
			if (fieldwidth == -1)
				fieldwidth = UUIDLENGTH;
			var = "uuid";
		}
		else if (str_starts_with(fmt, "index"))
		{
			asprintf(&field, "%d", this->index);
			if (fieldwidth == -1)
				fieldwidth = strlen(field);
			free_field = true;
			var = "index";
		}
		if (var != NULL)
			goto done;
	}

	/* try for an exposed variable */
	for (i=0; i<NVARS; i++)
	{
		if (vars[i].name == NULL)
			break;
		if (str_starts_with(fmt, vars[i].name))
		{
			field = var_value_message(&(vars[i]), 0);
			if (fieldwidth == -1)
				fieldwidth = strlen(field);
			var = vars[i].name;
			free_field = true;
			goto done;
			break;
		}
	}

	/* print uninterpreted */
	field = calloc(2, sizeof(char));
	fmt = orig_fmt;
	*field = *fmt;
	free_field = true;
	varlen = 1;
	fieldwidth = 1;
	goto done;

done:
	/* compute lengths */
	if (field == NULL)
		fieldlen = 0;
	if (fieldlen == -1)
		fieldlen = strlen(field);
	if (varlen == -1 && var != (char *)-1)
		varlen = strlen(var);

	if (right_align)
	{
		/* pad beginning of string */
		for (i=fieldlen; i<fieldwidth; i++)
			*(str+position+i-fieldlen) = ' ';

		/* copy string */
		if (field != NULL)
			strncpy(str+position+fieldwidth-fieldlen, field, fieldwidth);
	}
	else
	{
		/* copy string */
		if (field != NULL)
			strncpy(str+position, field, fieldwidth);

		/* pad end of string */
		for (i=fieldlen; i<fieldwidth; i++)
			*(str+position+i) = ' ';
	}
	*(str+position+fieldwidth) = 0;

	/* free field */
	if (free_field)
		free(field);

	return eval_string(maxlen, fmt+varlen, this, str, position+fieldwidth);
} /* }}} */

funcmap *find_function(const char *name, const prog_mode mode) /* {{{ */
{
	/* search through the function maps */
	int i;

	for (i=0; i<NFUNCS; i++)
	{
		if (str_eq(name, funcmaps[i].name) && (funcmaps[i].mode == MODE_ANY || mode == funcmaps[i].mode || mode == MODE_ANY))
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
			statusbar_message(cfg.statusbar_timeout, "search wrapped to top");
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

void force_redraw() /* {{{ */
{
	/* force a redraw of active windows */
	WINDOW *windows[] = {statusbar, tasklist, pager, header};
	const int nwins = sizeof(windows)/sizeof(WINDOW *);
	int i;

	for (i=0; i<nwins; i++)
	{
		wattrset(windows[i], COLOR_PAIR(0));
		if (windows[i]==NULL)
			continue;
		wipe_window(windows[i]);
		wnoutrefresh(windows[i]);
	}
	doupdate();

	print_header();
	tasklist_print_task_list();
	statusbar_message(cfg.statusbar_timeout, "redrawn");
} /* }}} */

void handle_resize() /* {{{ */
{
	/* handle a change in screen size */
	int pagerheight;

	/* make sure rows and cols are set correctly */
	rows = getmaxy(stdscr);
	cols = getmaxx(stdscr);

	/* resize windows */
	wresize(header, 1, cols);
	wresize(tasklist, rows-2, cols);
	wresize(statusbar, 1, cols);

	/* move to proper positions */
	mvwin(header, 0, 0);
	mvwin(tasklist, 1, 0);
	mvwin(statusbar, rows-1, 0);

	/* handle pager */
	if (pager != NULL)
	{
		pagerheight = getmaxy(pager);
		if (pagerheight > rows-2)
			pagerheight = rows-2;
		wresize(pager, pagerheight, cols);
		mvwin(pager, rows-pagerheight-1, 0);
	}

	/* redraw windows */
	tasklist_print_task_list();
	print_header();

	/* message about resize */
	tnc_fprintf(logfp, LOG_DEBUG, "window resized to y=%d x=%d", rows, cols);
	statusbar_message(cfg.statusbar_timeout, "window resized to y=%d x=%d", rows, cols);
} /* }}} */

void help(void) /* {{{ */
{
	/* print a list of options and program info */
	print_version();
	fprintf(stderr, "\nUsage: %s [options]\n\n", PROGNAME);
	fprintf(stderr, "  Options:\n"
			"    -l, --loglevel [value]   set log level\n"
			"    -d, --debug              debug mode\n"
			"    -f, --filter             set the initial filter\n"
			"    -h, --help               print this help message and exit\n"
			"    -v, --version            print the version and exit\n");
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
		wgetstr(statusbar, cmdstr);
		wipe_statusbar();

		/* reset */
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
		cmdstr = strdup(arg);

	/* run input command */
	if (cmdstr[0]!=0)
		handle_command(cmdstr);
	free(cmdstr);
} /* }}} */

void key_task_background_command(const char *arg) /* {{{ */
{
	/* run a background command */
	if (arg == NULL)
		return;
	task_background_command(arg);
	reload = 1;
} /* }}} */

void key_task_interactive_command(const char *arg) /* {{{ */
{
	/* run an interactive command */
	if (arg == NULL)
		return;
	task_interactive_command(arg);
	reload = 1;
} /* }}} */

void key_done() /* {{{ */
{
	/* exit tasknc */
	done = true;
} /* }}} */

char max_project_length() /* {{{ */
{
	/* compute max project length */
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

const char *name_function(void *function) /* {{{ */
{
	/* search through the function maps */
	int i;

	for (i=0; i<NFUNCS; i++)
	{
		if (function == funcmaps[i].function)
			return funcmaps[i].name;
	}

	return NULL;
} /* }}} */

void ncurses_end(int sig) /* {{{ */
{
	/* terminate ncurses */
	delwin(header);
	delwin(tasklist);
	delwin(statusbar);
	delwin(stdscr);
	endwin();

	switch (sig)
	{
		case SIGINT:
			tnc_fprintf(stdout, LOG_DEFAULT, "aborted");
			tnc_fprintf(logfp, LOG_DEBUG, "received SIGINT, exiting");
			break;
		case SIGSEGV:
			tnc_fprintf(logfp, LOG_ERROR, "SEGFAULT");
			tnc_fprintf(logfp, LOG_ERROR, "segmentation fault, exiting");
			break;
		case SIGKILL:
			tnc_fprintf(stdout, LOG_ERROR, "killed");
			tnc_fprintf(logfp, LOG_ERROR, "received SIGKILL, exiting");
			break;
		default:
			tnc_fprintf(stdout, LOG_DEFAULT, "done");
			tnc_fprintf(logfp, LOG_DEBUG, "exiting with code %d", sig);
			break;
	}

	cleanup();

	exit(0);
} /* }}} */

void ncurses_init() /* {{{ */
{
	/* initialize ncurses */
	int ret;

	/* register signals */
	signal(SIGINT, ncurses_end);
	signal(SIGKILL, ncurses_end);
	signal(SIGSEGV, ncurses_end);

	/* initialize screen */
	tnc_fprintf(stdout, LOG_DEBUG, "starting ncurses...");
	if ((stdscr = initscr()) == NULL )
	{
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
	}

	/* start colors */
	ret = init_colors();
	if (ret)
	{
		fprintf(stderr, "Error initializing colors.\n");
		tnc_fprintf(logfp, LOG_ERROR, "error initializing colors (%d)", ret);
	}
} /* }}} */

void print_header() /* {{{ */
{
	/* print the window title bar */
	char *tmp0;

	/* wipe bar and print bg color */
	wmove(header, 0, 0);
	wattrset(header, get_colors(OBJECT_HEADER, NULL, NULL));

	/* evaluate title string */
	tmp0 = (char *)eval_string(2*cols, cfg.formats.title, NULL, NULL, 0);
	umvaddstr_align(header, 0, tmp0);
	free(tmp0);

	wnoutrefresh(header);
} /* }}} */

void print_version(void) /* {{{ */
{
	/* print info about the currently running program */
	printf("%s %s by %s\n", PROGNAME, PROGVERSION, PROGAUTHOR);
} /* }}} */

void set_curses_mode(const ncurses_mode mode) /* {{{ */
{
	/* set curses settings for various common modes */
	switch (mode)
	{
		case NCURSES_MODE_STD:
			keypad(statusbar, TRUE);/* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			curs_set(0);            /* set cursor invisible */
			wtimeout(statusbar, cfg.nc_timeout);/* timeout getch */
			break;
		case NCURSES_MODE_STD_BLOCKING:
			keypad(statusbar, TRUE);/* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			curs_set(0);            /* set cursor invisible */
			wtimeout(statusbar, -1);/* no timeout on getch */
			break;
		case NCURSES_MODE_STRING:
			curs_set(1);            /* set cursor visible */
			nocbreak();             /* wait for \n */
			echo();                 /* echo input */
			wtimeout(statusbar, -1);/* no timeout on getch */
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
	if (stdscr == NULL)
	{
		if (cfg.loglvl >= LOG_DEBUG)
			tnc_fprintf(stdout, LOG_INFO, message);
		tnc_fprintf(logfp, LOG_DEBUG, "(stdscr==NULL) %s", message);
		free(message);
		return;
	}

	wipe_statusbar();

	/* print message */
	umvaddstr(statusbar, 0, 0, message);
	free(message);

	/* set timeout */
	if (dtmout>=0)
		sb_timeout = time(NULL) + dtmout;

	/* refresh now or at next doupdate depending on time */
	if (dtmout<0)
		wrefresh(statusbar);
	else
		wnoutrefresh(statusbar);
} /* }}} */

void statusbar_timeout() /* {{{ */
{
	/* timeout statusbar */
	if (sb_timeout>0 && sb_timeout<time(NULL))
	{
		sb_timeout = 0;
		wipe_statusbar();
	}
} /* }}} */

char *str_trim(char *str) /* {{{ */
{
	/* remove trailing and leading spaces from a string in place
	 * returns the pointer to the new start */
	char *pos;

	/* skip nulls */
	if (str==NULL)
		return NULL;

	/* leading */
	while (*str==' ' || *str=='\n' || *str=='\t')
		str++;
	if (*str == 0)
		return NULL;

	/* trailing */
	pos = str;
	while ((*pos)!=0)
		pos++;
	while (pos>str && (*(pos-1)==' ' || *(pos-1)=='\n' || *(pos-1)=='\t'))
	{
		*(pos-1) = 0;
		pos--;
	}

	return str;
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

	/* print background line */
	mvwhline(win, y, 0, ' ', cols);

	/* find break */
	pos = strstr(str, "$>");

	/* end left string */
	if (pos != NULL)
	{
		(*pos) = 0;
		right = (pos+2);
	}
	else
		right = NULL;

	/* print strings */
	tmp = umvaddstr(win, y, 0, str);
	ret = tmp;
	if (right != NULL)
		ret = umvaddstr(win, y, cols-strlen(right), right);
	if (tmp > ret)
		ret = tmp;

	/* fix string */
	if (pos != NULL)
		(*pos) = '$';

	return ret;
} /* }}} */

char *utc_date(const time_t timeint) /* {{{ */
{
	/* convert a utc time uint to a string */
	struct tm *tmr, *now;
	time_t cur;
	char *timestr;

	/* get current time */
	time(&cur);
	now = localtime(&cur);

	/* set time to either now or the specified time */
	tmr = timeint == 0  ?  now : localtime(&timeint);

	/* convert thte time to a formatted string */
	timestr = malloc(TIMELENGTH*sizeof(char));
	if (now->tm_year != tmr->tm_year)
		strftime(timestr, TIMELENGTH, "%F", tmr);
	else
		strftime(timestr, TIMELENGTH, "%b %d", tmr);

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

	if (printname == false)
		return value;

	message = malloc(strlen(v->name) + strlen(value) + 3);
	strcpy(message, v->name);
	strcat(message, ": ");
	strcat(message, value);
	free(value);

	return message;
} /* }}} */

void wipe_screen(WINDOW *win, const short startl, const short stopl) /* {{{ */
{
	/* clear specified lines of the screen */
	int y, x;

	wattrset(win, COLOR_PAIR(0));

	for (y=startl; y<=stopl; y++)
		for (x=0; x<cols; x++)
			mvwaddch(win, y, x, ' ');
} /* }}} */

void wipe_window(WINDOW *win) /* {{{ */
{
	/* wipe everything on the screen */
	int x, y, tx, ty;

	getmaxyx(win, y, x);

	for (ty=0; ty<y; ty++)
		for (tx=0; tx<x; tx++)
			mvwaddch(win, ty, tx, ' ');
	touchwin(win);
} /* }}} */

int main(int argc, char **argv) /* {{{ */
{
	/* declare variables */
	int c;
	bool debug = false;
	char *debugopts = NULL;

	/* open log */
	logfp = fopen(LOGFILE, "a");
	tnc_fprintf(logfp, LOG_DEBUG, "%s started", PROGNAME);

	/* set defaults */
	cfg.loglvl = LOGLVL_DEFAULT;
	setlocale(LC_ALL, "");

	/* handle arguments */
	static struct option long_options[] =
	{
		{"help",     no_argument,       0, 'h'},
		{"debug",    required_argument, 0, 'd'},
		{"version",  no_argument,       0, 'v'},
		{"loglevel", required_argument, 0, 'l'},
		{"filter",   required_argument, 0, 'f'},
		{0, 0, 0, 0}
	};
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "l:hvd:f:", long_options, &option_index)) != -1)
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
				debug = true;
				debugopts = strdup(optarg);
				break;
			case 'f':
				active_filter = strdup(optarg);
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

	/* run ncurses */
	if (!debug)
	{
		tnc_fprintf(logfp, LOG_DEBUG, "running gui");
		ncurses_init();
		mvwprintw(stdscr, 0, 0, "%s %s", PROGNAME, PROGVERSION);
		mvwprintw(stdscr, 1, 0, "configuring...");
		wrefresh(stdscr);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "configuring...");
		configure();
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "configuration complete");
		mvwprintw(stdscr, 1, 0, "loading tasks...");
		wrefresh(stdscr);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "loading tasks...");
		head = get_tasks(NULL);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "%d tasks loaded", taskcount);
		if (head==NULL)
		{
			ncurses_end(0);
			tnc_fprintf(stdout, LOG_WARN, "it appears that your task list is empty");
			tnc_fprintf(stdout, LOG_WARN, "please add some tasks for %s to manage\n", PROGNAME);
			return 1;
		}
		tasklist_window();
		ncurses_end(0);
	}

	/* debug mode */
	else
	{
		configure();
		head = get_tasks(NULL);
		test(debugopts);
		free(debugopts);
	}

	/* done */
	tnc_fprintf(logfp, LOG_DEBUG, "exiting");
	return 0;
} /* }}} */
