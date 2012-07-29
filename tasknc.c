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
#include <panel.h>
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
#include "tasklist.h"
#include "tasks.h"
#include "log.h"
#include "keys.h"
#include "pager.h"

/* global variables {{{ */
const char *progname = "tasknc";
const char *progversion = "v0.6";
const char *progauthor = "mjheagle";

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
bool resize;
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
	{"nc_timeout",        VAR_INT,  &(cfg.nc_timeout)},
	{"statusbar_timeout", VAR_INT,  &(cfg.statusbar_timeout)},
	{"log_level",         VAR_INT,  &(cfg.loglvl)},
	{"task_version",      VAR_STR,  &(cfg.version)},
	{"sort_mode",         VAR_CHAR, &(cfg.sortmode)},
	{"search_string",     VAR_STR,  &searchstring},
	{"selected_line",     VAR_INT,  &selline},
	{"task_count",        VAR_INT,  &taskcount},
	{"filter_string",     VAR_STR,  &active_filter},
	{"silent_shell",      VAR_CHAR, &(cfg.silent_shell)},
	{"program_name",      VAR_STR,  &progname},
	{"program_author",    VAR_STR,  &progauthor},
	{"program_version",   VAR_STR,  &progversion},
	{"title_format",      VAR_STR,  &(cfg.formats.title)},
	{"task_format",       VAR_STR,  &(cfg.formats.task)},
	{"view_format",       VAR_STR,  &(cfg.formats.view)},
};

funcmap funcmaps[] = {
	{"scroll_down", (void *)key_tasklist_scroll_down, 0, MODE_TASKLIST},
	{"scroll_down", (void *)key_pager_scroll_down,    0, MODE_PAGER},
	{"scroll_end",  (void *)key_tasklist_scroll_end,  0, MODE_TASKLIST},
	{"scroll_home", (void *)key_tasklist_scroll_home, 0, MODE_TASKLIST},
	{"scroll_up",   (void *)key_tasklist_scroll_up,   0, MODE_TASKLIST},
	{"scroll_up",   (void *)key_pager_scroll_up,      0, MODE_PAGER},
	{"reload",      (void *)key_tasklist_reload,      0, MODE_TASKLIST},
	{"undo",        (void *)key_tasklist_undo,        0, MODE_TASKLIST},
	{"add",         (void *)key_tasklist_add,         0, MODE_TASKLIST},
	{"modify",      (void *)key_tasklist_modify,      0, MODE_TASKLIST},
	{"sort",        (void *)key_tasklist_sort,        0, MODE_TASKLIST},
	{"search",      (void *)key_tasklist_search,      0, MODE_TASKLIST},
	{"search_next", (void *)key_tasklist_search_next, 0, MODE_TASKLIST},
	{"filter",      (void *)key_tasklist_filter,      0, MODE_TASKLIST},
	{"sync",        (void *)key_tasklist_sync,        0, MODE_TASKLIST},
	{"quit",        (void *)key_done,                 0, MODE_TASKLIST},
	{"quit",        (void *)key_pager_close,          0, MODE_PAGER},
	{"command",     (void *)key_command,              0, MODE_TASKLIST},
	{"stats",       (void *)view_stats,               0, MODE_TASKLIST},
	{"help",        (void *)help_window,              0, MODE_TASKLIST},
	{"view",        (void *)key_tasklist_view,        0, MODE_TASKLIST},
	{"edit",        (void *)key_tasklist_edit,        0, MODE_TASKLIST},
	{"complete",    (void *)key_tasklist_complete,    0, MODE_TASKLIST},
	{"delete",      (void *)key_tasklist_delete,      0, MODE_TASKLIST},
	{"set",         (void *)run_command_set,          1, MODE_TASKLIST},
	{"show",        (void *)run_command_show,         1, MODE_TASKLIST},
	{"bind",        (void *)run_command_bind,         1, MODE_TASKLIST},
	{"unbind",      (void *)run_command_unbind,       1, MODE_TASKLIST},
};
/* }}} */

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
			wattrset(stdscr, COLOR_PAIR(8));
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
	cfg.formats.title = calloc(64, sizeof(char));
	strcpy(cfg.formats.title, " $program_name ($selected_line/$task_count) $> $date");
	cfg.formats.task = calloc(64, sizeof(char));
	strcpy(cfg.formats.task, " $project $description $> $due");
	cfg.formats.view = calloc(64, sizeof(char));
	strcpy(cfg.formats.view, " task info");

	/* set initial filter */
	active_filter = calloc(32, sizeof(char));
	strcpy(active_filter, "status:pending");

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
	add_keybind(ERR,           NULL,                     NULL,            MODE_TASKLIST);
	add_keybind(ERR,           NULL,                     NULL,            MODE_PAGER);
	add_keybind('k',           key_tasklist_scroll_up,   NULL,            MODE_TASKLIST);
	add_keybind('k',           key_pager_scroll_up,      NULL,            MODE_PAGER);
	add_keybind(KEY_UP,        key_tasklist_scroll_up,   NULL,            MODE_TASKLIST);
	add_keybind('j',           key_tasklist_scroll_down, NULL,            MODE_TASKLIST);
	add_keybind('j',           key_pager_scroll_down,    NULL,            MODE_PAGER);
	add_keybind(KEY_DOWN,      key_tasklist_scroll_down, NULL,            MODE_TASKLIST);
	add_keybind(KEY_HOME,      key_tasklist_scroll_home, NULL,            MODE_TASKLIST);
	add_keybind('g',           key_tasklist_scroll_home, NULL,            MODE_TASKLIST);
	add_keybind(KEY_END,       key_tasklist_scroll_end,  NULL,            MODE_TASKLIST);
	add_keybind('G',           key_tasklist_scroll_end,  NULL,            MODE_TASKLIST);
	add_keybind('e',           key_tasklist_edit,        NULL,            MODE_TASKLIST);
	add_keybind('r',           key_tasklist_reload,      NULL,            MODE_TASKLIST);
	add_keybind('u',           key_tasklist_undo,        NULL,            MODE_TASKLIST);
	add_keybind('d',           key_tasklist_delete,      NULL,            MODE_TASKLIST);
	add_keybind('c',           key_tasklist_complete,    NULL,            MODE_TASKLIST);
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
	add_keybind(':',           key_command,              NULL,            MODE_TASKLIST);
	add_keybind('h',           help_window,              NULL,            MODE_TASKLIST);

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
			fieldwidth = cfg.fieldlengths.description;
			fieldlen = strlen(this->description);
			varlen = strlen("description");
		}
		else if (str_starts_with(fmt+1, "project"))
		{
			strcpy(str+position, this->project);
			fieldwidth = cfg.fieldlengths.project;
			fieldlen = strlen(this->project);
			varlen = strlen("project");
		}
		for (i=fieldlen; i<fieldwidth; i++)
			*(str+position+i) = ' ';
		*(str+position+fieldwidth) = 0;
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

funcmap *find_function(const char *name, const prog_mode mode) /* {{{ */
{
	/* search through the function maps */
	int i;

	for (i=0; i<NFUNCS; i++)
	{
		if (str_eq(name, funcmaps[i].name) && (mode == funcmaps[i].mode || mode == MODE_ANY))
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
	funcmap *fmap;

	tnc_fprintf(logfp, LOG_DEBUG, "command received: %s", cmdstr);

	/* determine command */
	pos = strchr(cmdstr, ' ');

	/* split off arguments */
	if (pos!=NULL)
	{
		(*pos) = 0;
		args = ++pos;
	}

	/* log command */
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: %s", cmdstr);
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "command: [args] %s", args);

	/* handle command & arguments */
	/* try for exposed command */
	fmap = find_function(cmdstr, MODE_ANY);
	if (fmap!=NULL)
	{
		(fmap->function)(str_trim(args));
		return;
	}
	/* version: print version string */
	if (str_eq(cmdstr, "version"))
		statusbar_message(cfg.statusbar_timeout, "%s %s by %s\n", progname, progversion, progauthor);
	/* quit/exit: exit tasknc */
	else if (str_eq(cmdstr, "quit") || str_eq(cmdstr, "exit"))
		done = 1;
	/* reload: force reload of task list */
	else if (str_eq(cmdstr, "reload"))
	{
		reload = 1;
		statusbar_message(cfg.statusbar_timeout, "task list reloaded");
	}
	/* redraw: force redraw of screen */
	else if (str_eq(cmdstr, "redraw"))
		redraw = 1;
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

void handle_keypress(const int c, const prog_mode mode) /* {{{ */
{
	/* handle a key press on the main screen */
	keybind *this_bind;
	char *modestr;

	this_bind = keybinds;
	while (this_bind!=NULL)
	{
		if (this_bind->mode == mode && c == this_bind->key)
		{
			if (this_bind->function != NULL)
			{
				if (this_bind->mode == MODE_PAGER)
					modestr = "pager - ";
				else if (this_bind->mode == MODE_TASKLIST)
					modestr = "tasklist - ";
				else
					modestr = " ";
				tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "calling function @%p %s%s", this_bind->function, modestr, name_function(this_bind->function));
				(*(this_bind->function))(this_bind->argstr);
			}
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
	/* exit tasknc */
	done = 1;
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

void ncurses_colors(void) /* {{{ */
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
	tnc_fprintf(stdout, LOG_DEBUG, "starting ncurses...");
	signal(SIGINT, ncurses_end);
	signal(SIGKILL, ncurses_end);
	signal(SIGSEGV, ncurses_end);
	if ((stdscr = initscr()) == NULL ) {
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
	}

} /* }}} */

void print_header() /* {{{ */
{
	/* print the window title bar */
	char *tmp0;

	/* wipe bar and print bg color */
	wmove(header, 0, 0);
	wattrset(header, COLOR_PAIR(1));

	/* evaluate title string */
	tmp0 = (char *)eval_string(2*cols, cfg.formats.title, NULL, NULL, 0);
	umvaddstr_align(header, 0, tmp0);
	free(tmp0);

	wnoutrefresh(header);
} /* }}} */

void print_version(void) /* {{{ */
{
	/* print info about the currently running program */
	printf("%s %s by %s\n", progname, progversion, progauthor);
} /* }}} */

void run_command_bind(char *args) /* {{{ */
{
	/* create a new keybind */
	int key;
	char *function, *arg, *keystr, *modestr, *aarg;
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
	{
		aarg = calloc((int)strlen(arg), sizeof(char));
		strcpy(aarg, arg);
	}
	else
		aarg = NULL;

	/* add keybind */
	add_keybind(key, func, str_trim(aarg), mode);
	statusbar_message(cfg.statusbar_timeout, "key bound");
} /* }}} */

void run_command_unbind(char *argstr) /* {{{ */
{
	/* handle a keyboard instruction to unbind a key */
	char *modestr, *keystr;

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
			keypad(statusbar, TRUE);/* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			ncurses_colors();            /* initialize colors */
			curs_set(0);            /* set cursor invisible */
			wtimeout(statusbar, cfg.nc_timeout);/* timeout getch */
			break;
		case NCURSES_MODE_STD_BLOCKING:
			keypad(statusbar, TRUE);/* enable keyboard mapping */
			nonl();                 /* tell curses not to do NL->CR/NL on output */
			cbreak();               /* take input chars one at a time, no wait for \n */
			noecho();               /* dont echo input */
			ncurses_colors();            /* initialize colors */
			curs_set(0);            /* set cursor invisible */
			wtimeout(statusbar, -1);/* no timeout on getch */
			break;
		case NCURSES_MODE_STRING:
			curs_set(2);            /* set cursor visible */
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
	if (stdscr==NULL)
	{
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

	/* trailing */
	pos = str;
	while ((*pos)!=0)
		pos++;
	while (*(pos-1)==' ' || *(pos-1)=='\n' || *(pos-1)=='\t')
		*(pos-1) = 0;

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
	if (right != NULL)
		ret = umvaddstr(win, y, cols-strlen(right), right);
	if (tmp > ret)
		ret = tmp;

	/* fix string */
	if (pos != NULL)
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

void wipe_screen(WINDOW *win, const short startl, const short stopl) /* {{{ */
{
	/* clear specified lines of the screen */
	int y, x;

	wattrset(win, COLOR_PAIR(0));

	for (y=startl; y<=stopl; y++)
		for (x=0; x<cols; x++)
			mvwaddch(win, y, x, ' ');
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
		ncurses_init();
		tasklist_window();
		ncurses_end(0);
	}

	/* debug mode */
	else
	{
		task_count();
		printf("task count: %d\n", taskcount);
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
