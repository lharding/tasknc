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
#include "config.h"

/* macros {{{ */
/* wiping functions */
#define wipe_tasklist()                 wipe_screen(1, size[1]-2)
#define wipe_statusbar()                wipe_screen(size[1]-1, size[1]-1)

/* string comparison */
#define str_starts_with(x, y)           (strncmp((x),(y),strlen(y)) == 0) 
#define str_eq(x, y)                    (strcmp((x), (y))==0)
#define check_free(x)                   if (x!=NULL) free(x);

/* program information */
#define NAME                            "taskwarrior ncurses shell"
#define SHORTNAME                       "tasknc"
#define VERSION                         "0.6"
#define AUTHOR                          "mjheagle"

/* field lengths */
#define UUIDLENGTH                      38
#define DATELENGTH                      10

/* action definitions */
typedef enum {
	ACTION_EDIT = 0,
	ACTION_COMPLETE,
	ACTION_DELETE,
	ACTION_VIEW
} task_action_type;

/* ncurses settings */
typedef enum {
	NCURSES_MODE_STD = 0,
	NCURSES_MODE_STD_BLOCKING,
	NCURSES_MODE_STRING
} ncurses_mode;

/* log levels */
typedef enum {
	LOG_DEFAULT = 0,
	LOG_ERROR,
	LOG_DEBUG,
	LOG_DEBUG_VERBOSE
} log_mode;

/* var struct management */
typedef enum {
	VAR_UNDEF = 0,
	VAR_CHAR,
	VAR_STR,
	VAR_INT
} var_type;
#define NVARS                           (int)(sizeof(vars)/sizeof(var))
#define NFUNCS                          (int)(sizeof(funcmaps)/sizeof(funcmap))

/* regex options */
#define REGEX_OPTS REG_ICASE|REG_EXTENDED|REG_NOSUB|REG_NEWLINE

/* default settings */
#define STATUSBAR_TIMEOUT_DEFAULT       3
#define NCURSES_WAIT                    500
#define LOGLVL_DEFAULT                  0
/* }}} */

/* data struct definitions {{{ */
typedef struct _task
{
	unsigned short index;
	char *uuid;
	char *tags;
	unsigned int start;
	unsigned int end;
	unsigned int entry;
	unsigned int due;
	char *project;
	char priority;
	char *description;
	struct _task *prev;
	struct _task *next;
} task;

typedef struct _var
{
	char *name;
	var_type type;
	void *ptr;
} var;

typedef struct _funcmap
{
	char *name;
	void (*function)();
	int argn;
} funcmap;

typedef struct _bind
{
	int key;
	void (*function)();
	int argint;
	char *argstr;
	struct _bind *next;
} keybind;
/* }}} */

/* function prototypes {{{ */
static void add_int_keybind(int, void *, int);
static void add_keybind(int, void *, char *);
static void check_curs_pos(void);
static void check_screen_size();
static void cleanup();
static char compare_tasks(const task *, const task *, const char);
static void configure(void);
static funcmap *find_function(const char *);
static void find_next_search_result(task *, task *);
static var *find_var(const char *);
static char free_task(task *);
static void free_tasks(task *);
static unsigned short get_task_id(char *);
static task *get_tasks(char *);
static void handle_command(char *);
static void handle_keypress(int);
static void help(void);
static void key_add();
static void key_command(const char *);
static void key_done();
static void key_filter(const char *);
static void key_reload();
static void key_scroll(const int);
static void key_search(const char *);
static void key_search_next();
static void key_sort();
static void key_sync();
static void key_task_action(const task_action_type, const char *, const char *);
static void key_undo();
static void logmsg(const log_mode, const char *, ...) __attribute__((format(printf,2,3)));
static task *malloc_task(void);
static bool match_string(const char *, const char *);
static char max_project_length();
static void nc_colors(void);
static void nc_end(int);
static void nc_main();
static int parse_key(const char *);
static task *parse_task(char *);
static void print_task(int, task *);
static void print_task_list();
static void print_title();
static void print_version(void);
static void reload_task(task *);
static void reload_tasks();
static void remove_char(char *, char);
static int remove_keybinds(const int);
static void run_command_bind(char *);
static void run_command_set(char *);
static void run_command_show(const char *);
static void set_curses_mode(const ncurses_mode);
static void sort_tasks(task *, task *);
static void sort_wrapper(task *);
static void statusbar_message(const int, const char *, ...) __attribute__((format(printf,2,3)));
static char *str_trim(char *);
static void swap_tasks(task *, task *);
static int task_action(const task_action_type);
static void task_add(void);
static void task_count();
static bool task_match(const task *, const char *);
int umvaddstr(const int, const int, const char *, ...) __attribute__((format(printf,3,4)));
static char *utc_date(const unsigned int);
static char *var_value_message(var *);
static void wipe_screen(const short, const short);
/* }}} */

/* runtime config {{{ */
struct {
	int nc_timeout;
	int statusbar_timeout;
	log_mode loglvl;
	char *version;
	char sortmode;
	char silent_shell;
} cfg;
/* }}} */

/* run state structs {{{ */
struct {
	short project;
	short description;
	short date;
} fieldlengths;

struct {
	char redraw;
	char reload;
	char done;
} state;
/* }}} */

/* global variables {{{ */
short pageoffset = 0;                   /* number of tasks page is offset */
time_t sb_timeout = 0;                  /* when statusbar should be cleared */
char *searchstring = NULL;              /* currently active search string */
short selline = 0;                      /* selected line number */
int size[2];                            /* size of the ncurses window */
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
	{"silent_shell",      VAR_CHAR, &(cfg.silent_shell)}
};

funcmap funcmaps[] = {
	{"scroll",      (void *)key_scroll,      1},
	{"task_action", (void *)key_task_action, 1},
	{"reload",      (void *)key_reload,      0},
	{"undo",        (void *)key_undo,        0},
	{"add",         (void *)key_add,         0},
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
	logmsg(LOG_DEBUG, "bind #%d: key %c (%d) bound to @%p (args: %d/%s)", n, key, key, function,new->argint, new->argstr);
} /* }}} */

void check_curs_pos(void) /* {{{ */
{
	/* check if the cursor is in a valid position */
	const short onscreentasks = size[1]-3;

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
	logmsg(LOG_DEBUG_VERBOSE, "selline:%d offset:%d taskcount:%d perscreen:%d", selline, pageoffset, taskcount, size[1]-3);
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
			refresh();
			attrset(COLOR_PAIR(0));
			usleep(100000);
		}
		count++;
		getmaxyx(stdscr, size[1], size[0]);
	} while (size[0]<DATELENGTH+20+fieldlengths.project || size[1]<5);
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
	fclose(logfp);
} /* }}} */

char compare_tasks(const task *a, const task *b, const char sort_mode) /* {{{ */
{
	/* compare two tasks to determine order
	 * a return of 1 means that the tasks should be swapped (b comes before a)
	 */
	char ret = 0;
	int tmp;

	/* determine sort algorithm and apply it */
	switch (sort_mode)
	{
		case 'n':       // sort by index
			if (a->index<b->index)
				ret = 1;
			break;
		default:
		case 'p':       // sort by project name => index
			if (a->project == NULL)
			{
				if (b->project != NULL)
					ret = 1;
				break;
			}
			if (b->project == NULL)
				break;
			tmp = strcmp(a->project, b->project);
			if (tmp<0)
				ret = 1;
			if (tmp==0)
				ret = compare_tasks(a, b, 'n');
			break;
		case 'd':       // sort by due date => priority => project => index
			if (a->due == 0)
			{
				if (b->due == 0)
					ret = compare_tasks(a, b, 'r');
				break;
			}
			if (b->due == 0)
			{
				ret = 1;
				break;
			}
			if (a->due<b->due)
				ret = 1;
			break;
		case 'r':       // sort by priority => project => index
			if (a->priority == 0)
			{
				if (b->priority == 0)
					ret = compare_tasks(a, b, 'p');
				break;
			}
			if (b->priority == 0)
			{
				ret = 1;
				break;
			}
			if (a->priority == b->priority)
			{
				ret = compare_tasks(a, b, 'p');
				break;
			}
			switch (b->priority)
			{
				case 'H':
				default:
					break;
				case 'M':
					if (a->priority=='H')
						ret = 1;
					break;
				case 'L':
					if (a->priority=='M' || a->priority=='H')
						ret = 1;
					break;
			}
			break;
	}

	return ret;
} /* }}} */

void configure(void) /* {{{ */
{
	/* parse config file to get runtime options */
	FILE *cmd, *config;
	char line[TOTALLENGTH], *filepath, *xdg_config_home, *home;
	int ret;

	/* set default values */
	cfg.nc_timeout = NCURSES_WAIT;                          /* time getch will wait */
	cfg.statusbar_timeout = STATUSBAR_TIMEOUT_DEFAULT;      /* default time before resetting statusbar */
	cfg.sortmode = 'd';                                     /* determine sort algorithm */
	cfg.silent_shell = 0;                                   /* determine whether shell commands should be visible */

	/* get task version */
	cmd = popen("task version rc._forcecolor=no", "r");
	while (fgets(line, sizeof(line)-1, cmd) != NULL)
	{
		cfg.version = calloc(8, sizeof(char));
		ret = sscanf(line, "task %[^ ]* ", cfg.version);
		if (ret>0)
		{
			logmsg(LOG_DEBUG, "task version: %s", cfg.version);
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
	logmsg(LOG_DEBUG, "config file: %s", filepath);

	/* free filepath */
	free(filepath);

	/* check for a valid fd */
	if (config == NULL)
	{
		puts("config file could not be opened");
		logmsg(LOG_ERROR, "config file could not be opened");
		return;
	}

	/* read config file */
	logmsg(LOG_DEBUG, "reading config file");
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
			logmsg(LOG_DEBUG_VERBOSE, "search wrapped");
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

char free_task(task *tsk) /* {{{ */
{
	/* free the memory allocated to a task */
	char ret = 0;

	free(tsk->uuid);
	if (tsk->tags!=NULL)
		free(tsk->tags);
	if (tsk->project!=NULL)
		free(tsk->project);
	if (tsk->description!=NULL)
		free(tsk->description);
	free(tsk);

	return ret;
} /* }}} */

void free_tasks(task *head) /* {{{ */
{
	/* free the task stack */
	task *cur, *next;

	cur = head;
	while (cur!=NULL)
	{
		next = cur->next;
		free_task(cur);
		cur = next;
	}
} /* }}} */

task *get_task_by_position(int n) /* {{{ */
{
	/* navigate to line n
	 * and return its task pointer
	 */
	task *cur;
	short i = -1;

	cur = head;
	while (cur!=NULL)
	{
		i++;
		if (i==n)
			break;
		cur = cur->next;
	}

	return cur;
} /* }}} */

unsigned short get_task_id(char *uuid) /* {{{ */
{
	/* given a task uuid, find its id
	 * we do this using a custom report
	 * necessary to do without uuid addressing in task v2
	 */
	FILE *cmd;
	char line[128], format[128];
	int ret;
	unsigned short id = 0;

	/* generate format to scan for */
	sprintf(format, "%s %%hu", uuid);

	/* run command */
	cmd = popen("task rc.report.all.columns:uuid,id rc.report.all.labels:UUID,id rc.report.all.sort:id- all status:pending rc._forcecolor=no", "r");
	while (fgets(line, sizeof(line)-1, cmd) != NULL)
	{
		ret = sscanf(line, format, &id);
		if (ret>0)
			break;
	}
	pclose(cmd);

	return id;
} /* }}} */

task *get_tasks(char *uuid) /* {{{ */
{
	FILE *cmd;
	char *line, *tmp, *cmdstr;
	int linelen = TOTALLENGTH;
	unsigned short counter = 0;
	task *last;

	/* generate & run command */
	cmdstr = calloc(64, sizeof(char));
	if (cfg.version[0]<'2')
		strcat(cmdstr, "task export.json status:pending");
	else
		strcat(cmdstr, "task export status:pending");
	if (active_filter!=NULL)
	{
		strcat(cmdstr, " ");
		strcat(cmdstr, active_filter);
	}
	if (uuid!=NULL)
	{
		strcat(cmdstr, " ");
		strcat(cmdstr, uuid);
	}
	logmsg(LOG_DEBUG, "reloading tasks (%s)", cmdstr);
	cmd = popen(cmdstr, "r");
	free(cmdstr);

	/* parse output */
	last = NULL;
	head = NULL;
	line = calloc(linelen, sizeof(char));
	while (fgets(line, linelen-1, cmd) != NULL)
	{
		task *this;

		/* check for longer lines */
		while (strchr(line, '\n')==NULL)
		{
			linelen += TOTALLENGTH;
			line = realloc(line, linelen*sizeof(char));
			tmp = calloc(TOTALLENGTH, sizeof(char));
			if (fgets(tmp, TOTALLENGTH-1, cmd)==NULL)
				break;
			strcat(line, tmp);
			free(tmp);
		}

		/* remove escapes */
		remove_char(line, '\\');

		/* log line */
		logmsg(LOG_DEBUG_VERBOSE, line);

		/* parse line */
		this = parse_task(line);
		if (this == NULL)
			return NULL;
		else if (this == (task *)-1)
			continue;
		else if (this->uuid == NULL ||
				this->description == NULL)
			return NULL;

		/* set pointers */
		this->index = counter;
		this->prev = last;

		if (counter==0)
			head = this;
		if (counter>0)
			last->next = this;
		last = this;
		counter++;
		logmsg(LOG_DEBUG_VERBOSE, "uuid: %s", this->uuid);
		logmsg(LOG_DEBUG_VERBOSE, "description: %s", this->description);
		logmsg(LOG_DEBUG_VERBOSE, "project: %s", this->project);
		logmsg(LOG_DEBUG_VERBOSE, "tags: %s", this->tags);

		/* prepare a new line */
		free(line);
		line = calloc(linelen, sizeof(char));
	}
	free(line);
	pclose(cmd);


	/* sort tasks */
	if (head!=NULL)
		sort_wrapper(head);

	return head;
} /* }}} */

void handle_command(char *cmdstr) /* {{{ */
{
	/* accept a command string, determine what action to take, and execute */
	char *pos, *args = NULL;
	cmdstr = str_trim(cmdstr);

	logmsg(LOG_DEBUG, "command received: %s", cmdstr);

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
		statusbar_message(cfg.statusbar_timeout, "%s v%s by %s\n", NAME, VERSION, AUTHOR);
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
			logmsg(-1, "uuid: %s", this->uuid);
			logmsg(-1, "description: %s", this->description);
			logmsg(-1, "project: %s", this->project);
			logmsg(-1, "tags: %s", this->tags);
			this = this->next;
		}
	}
	/* bind: add a new keybind */
	else if (str_eq(cmdstr, "bind"))
		run_command_bind(str_trim(args));
	else
	{
		statusbar_message(cfg.statusbar_timeout, "error: command %s not found", cmdstr);
		logmsg(LOG_ERROR, "error: command %s not found", cmdstr);
	}

	/* debug */
	logmsg(LOG_DEBUG_VERBOSE, "command: %s", cmdstr);
	logmsg(LOG_DEBUG_VERBOSE, "command: [args] %s", args);
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
			logmsg(LOG_DEBUG_VERBOSE, "calling function @%p", this_bind->function);
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
	fprintf(stderr, "\nUsage: %s [options]\n\n", NAME);
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
	def_prog_mode();
	endwin();
	task_add();
	refresh();
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
		cmdstr = calloc(size[0], sizeof(char));
		getstr(cmdstr);
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
		active_filter = calloc(2*size[0], sizeof(char));
		getstr(active_filter);
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
				if (selline>=pageoffset+size[1]-2)
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
			pageoffset = taskcount-size[1]+2;
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
		searchstring = malloc((size[0]-16)*sizeof(char));
		getstr(searchstring);
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

void key_sort() /* {{{ */
{
	/* handle a keyboard direction to sort */
	char m;

	attrset(COLOR_PAIR(0));
	statusbar_message(cfg.statusbar_timeout, "enter sort mode: iNdex, Project, Due, pRiority");
	set_curses_mode(NCURSES_MODE_STD_BLOCKING);

	m = getch();
	set_curses_mode(NCURSES_MODE_STD);
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
	refresh();
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
	refresh();
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
	refresh();
	if (ret==0)
	{
		statusbar_message(cfg.statusbar_timeout, "undo executed");
		state.reload = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "undo execution failed (%d)", ret);
} /* }}} */

void logmsg(const log_mode minloglvl, const char *format, ...) /* {{{ */
{
	/* log a message to the logfile */
	time_t lt;
	struct tm *t;
	va_list args;
	int ret;
	const int timesize = 32;
	char timestr[timesize];

	/* determine if msg should be logged */
	if (minloglvl>cfg.loglvl)
		return;

	/* get time */
	lt = time(NULL);
	t = localtime(&lt);
	ret = strftime(timestr, timesize, "%F %H:%M:%S", t);
	if (ret==0)
		return;

	/* timestamp */
	fprintf(logfp, "[%s] ", timestr);

	/* log type header */
	switch(minloglvl)
	{
		case LOG_ERROR:
			fputs("ERROR: ", logfp);
			break;
		case LOG_DEBUG:
		case LOG_DEBUG_VERBOSE:
			fputs("DEBUG: ", logfp);
		default:
			break;
	}

	/* write log entry */
	va_start(args, format);
	vfprintf(logfp, format, args);
	va_end(args);

	/* trailing newline */
	fputc('\n', logfp);
} /* }}} */

task *malloc_task(void) /* {{{ */
{
	/* allocate memory for a new task
	 * and initialize values where necessary
	 */
	task *tsk = calloc(1, sizeof(task));
	if (tsk==NULL)
		return NULL;

	tsk->index = 0;
	tsk->uuid = NULL;
	tsk->tags = NULL;
	tsk->start = 0;
	tsk->end = 0;
	tsk->entry = 0;
	tsk->due = 0;
	tsk->project = NULL;
	tsk->priority = 0;
	tsk->description = NULL;
	tsk->next = NULL;
	tsk->prev = NULL;

	return tsk;
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

	return len+1;
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
			puts("aborted");
			logmsg(LOG_DEBUG, "received SIGINT, exiting");
			break;
		case SIGSEGV:
			puts("SEGFAULT");
			logmsg(LOG_DEFAULT, "segmentation fault, exiting");
			break;
		case SIGKILL:
			puts("killed");
			logmsg(LOG_DEFAULT, "received SIGKILL, exiting");
			break;
		default:
			puts("done");
			logmsg(LOG_DEBUG, "exiting with code %d", sig);
			break;
	}

	cleanup();

	exit(0);
} /* }}} */

void nc_main() /* {{{ */
{
	/* ncurses main function */
	WINDOW *stdscr;
	int c, tmp, oldsize[2];
	fieldlengths.project = max_project_length();
	fieldlengths.date = DATELENGTH;

	/* initialize ncurses */
	puts("starting ncurses...");
	signal(SIGINT, nc_end);
	signal(SIGSEGV, nc_end);
	if ((stdscr = initscr()) == NULL ) {
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
	}

	/* set curses settings */
	set_curses_mode(NCURSES_MODE_STD);

	/* print main screen */
	check_screen_size();
	getmaxyx(stdscr, oldsize[1], oldsize[0]);
	fieldlengths.description = oldsize[0]-fieldlengths.project-1-fieldlengths.date;
	task_count();
	print_title(oldsize[0]);
	attrset(COLOR_PAIR(0));
	print_task_list();
	refresh();

	/* main loop */
	while (1)
	{
		/* set variables for determining actions */
		state.done = 0;
		state.redraw = 0;
		state.reload = 0;

		/* get the screen size */
		getmaxyx(stdscr, size[1], size[0]);

		/* check for a screen thats too small */
		check_screen_size();

		/* check for size changes */
		if (size[0]!=oldsize[0] || size[1]!=oldsize[1])
		{
			state.redraw = 1;
			wipe_statusbar();
		}
		for (tmp=0; tmp<2; tmp++)
			oldsize[tmp] = size[tmp];

		/* get a character */
		c = getch();

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
			fieldlengths.description = size[0]-fieldlengths.project-1-fieldlengths.date;
			print_title();
			print_task_list();
			check_curs_pos();
			refresh();
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

task *parse_task(char *line) /* {{{ */
{
	task *tsk = malloc_task();
	char *token, *tmpstr, *tmpcontent;
	tmpcontent = NULL;
	int tokencounter = 0;

	/* detect lines that are not json */
	if (line[0]!='{')
		return (task *) -1;

	/* parse json */
	token = strtok(line, ",");
	while (token != NULL)
	{
		char *field, *content, *divider, endchar;
		struct tm tmr;

		/* increment counter */
		tokencounter++;

		/* determine field name */
		if (token[0] == '{')
			token++;
		if (token[0] == '"')
			token++;
		divider = strchr(token, ':');
		if (divider==NULL)
			break;
		(*divider) = '\0';
		(*(divider-1)) = '\0';
		field = token;

		/* get content */
		content = divider+2;
		if (str_eq(field, "tags") || str_eq(field, "annotations"))
			endchar = ']';
		else if (str_eq(field, "id"))
		{
			token = strtok(NULL, ",");
			continue;
		}
		else
			endchar = '"';

		divider = strchr(content, endchar);
		if (divider!=NULL)
			(*divider) = '\0';
		else /* handle commas */
		{
			tmpcontent = malloc((strlen(content)+1)*sizeof(char));
			strcpy(tmpcontent, content);
			do
			{
				token = strtok(NULL, ",");
				tmpcontent = realloc(tmpcontent, (strlen(tmpcontent)+strlen(token)+5)*sizeof(char));
				strcat(tmpcontent, ",");
				strcat(tmpcontent, token);
				divider = strchr(tmpcontent, endchar);
			} while (divider==NULL);
			(*divider) = '\0';
			content = tmpcontent;
		}

		/* log content */
		logmsg(LOG_DEBUG_VERBOSE, "field: %s; content: %s", field, content);

		/* handle data */
		if (str_eq(field, "uuid"))
		{
			tsk->uuid = malloc(UUIDLENGTH*sizeof(char));
			strcpy(tsk->uuid, content);
		}
		else if (str_eq(field, "project"))
		{
			tsk->project = malloc(PROJECTLENGTH*sizeof(char));
			strcpy(tsk->project, content);
		}
		else if (str_eq(field, "description"))
		{
			tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
			strcpy(tsk->description, content);
		}
		else if (str_eq(field, "priority"))
			tsk->priority = content[0];
		else if (str_eq(field, "due"))
		{
			memset(&tmr, 0, sizeof(tmr));
			strptime(content, "%Y%m%dT%H%M%S%z", &tmr);
			tmpstr = malloc(32*sizeof(char));
			strftime(tmpstr, 32, "%s", &tmr);
			sscanf(tmpstr, "%d", &(tsk->due));
			free(tmpstr);
		}
		else if (str_eq(field, "tags"))
		{
			tsk->tags = malloc(TAGSLENGTH*sizeof(char));
			strcpy(tsk->tags, content);
		}

		/* free tmpcontent if necessary */
		if (tmpcontent!=NULL)
		{
			free(tmpcontent);
			tmpcontent = NULL;
		}

		/* move to the next token */
		token = strtok(NULL, ",");
	}

	if (tokencounter<2)
	{
		free_tasks(tsk);
		return NULL;
	}
	else
		return tsk;
} /* }}} */

void print_task(int tasknum, task *this) /* {{{ */
{
	/* print a task specified by number */
	char sel = 0;
	int x, y;

	/* determine position to print */
	y = tasknum-pageoffset+1;
	if (y<=0 || y>=size[1]-1)
		return;

	/* find task pointer if necessary */
	if (this==NULL)
		this = get_task_by_position(tasknum);

	/* determine if line is selected */
	if (tasknum==selline)
		sel = 1;

	/* wipe line */
	attrset(COLOR_PAIR(0));
	for (x=0; x<size[0]; x++)
		mvaddch(y, x, ' ');

	/* print project */
	attrset(COLOR_PAIR(2+sel));
	if (this->project==NULL)
		umvaddstr(y, fieldlengths.project-1, " ");
	else
		umvaddstr(y, fieldlengths.project-strlen(this->project), this->project);

	/* print description */
	umvaddstr(y, fieldlengths.project+1, this->description);

	/* print due date or priority if available */
	if (this->due != 0)
	{
		char *tmp;
		tmp = utc_date(this->due);
		umvaddstr(y, size[0]-strlen(tmp), tmp);
		free(tmp);
	}
	else if (this->priority)
	{
		mvaddch(y, size[0]-1, this->priority);
	}
	else
		mvaddch(y, size[0]-1, ' ');
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
	if (counter<size[0]-2)
		wipe_screen(counter+1-pageoffset, size[0]-1);
} /* }}} */

void print_title() /* {{{ */
{
	/* print the window title bar */
	char *tmp0;
	int x;

	/* wipe bar and print bg color */
	attrset(COLOR_PAIR(1));
	for (x=0; x<size[0]; x++)
		mvaddch(0, x, ' ');

	/* print program info */
	tmp0 = calloc(size[0], sizeof(char));
	snprintf(tmp0, size[0], "%s v%s  (%d/%d)", SHORTNAME, VERSION, selline+1, totaltaskcount);
	umvaddstr(0, 0, tmp0);
	free(tmp0);

	/* print the current date */
	tmp0 = utc_date(0);
	umvaddstr(0, size[0]-strlen(tmp0), tmp0);
	free(tmp0);
} /* }}} */

void print_version(void) /* {{{ */
{
	/* print info about the currently running program */
	printf("%s v%s by %s\n", NAME, VERSION, AUTHOR);
} /* }}} */

void reload_task(task *this) /* {{{ */
{
	/* reload an individual task's data by number */
	task *new;

	/* get new task */
	new = get_tasks(this->uuid);

	/* transfer pointers */
	new->prev = this->prev;
	new->next = this->next;
	if (this->prev!=NULL)
		this->prev->next = new;
	if (this->next!=NULL)
		this->next->prev = new;

	/* move to head if necessary */
	if (this==head)
		head = new;

	/* free old task */
	free_task(this);
} /* }}} */

void reload_tasks() /* {{{ */
{
	/* reset head with a new list of tasks */
	task *cur;

	logmsg(LOG_DEBUG, "reloading tasks");

	free_tasks(head);

	head = get_tasks(NULL);

	/* debug */
	cur = head;
	while (cur!=NULL)
	{
		logmsg(LOG_DEBUG_VERBOSE, "%d,%s,%s,%d,%d,%d,%d,%s,%c,%s", cur->index, cur->uuid, cur->tags, cur->start, cur->end, cur->entry, cur->due, cur->project, cur->priority, cur->description);
		cur = cur->next;
	}
} /* }}} */

void remove_char(char *str, char remove) /* {{{ */
{
	/* iterate through a string and remove escapes inline */
	const int len = strlen(str);
	int i, offset = 0;

	for (i=0; i<len; i++)
	{
		if (str[i+offset]=='\0')
			break;
		str[i] = str[i+offset];
		while (str[i]==remove || str[i]=='\0')
		{
			offset++;
			str[i] = str[i+offset];
		}
		if (str[i+offset]=='\0')
			break;
	}

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
		logmsg(LOG_ERROR, "bind: key must be specified (%s)", args);
		return;
	}

	/* split off key */
	keystr = args;
	function = strchr(keystr, ' ');
	if (function==NULL)
	{
		logmsg(LOG_ERROR, "bind: function must be specified (%s)", args);
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
		logmsg(LOG_ERROR, "bind: invalid function specified (%s)", args);
		return;
	}
	func = fmap->function;

	/* error out if there is no argument specified when required */
	if (fmap->argn>0 && arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "bind: argument required for function %s", function);
		return;
	}

	/* add keybind */
	aarg = calloc((int)strlen(arg), sizeof(char));
	strcpy(aarg, arg);
	add_keybind(key, func, aarg);
	statusbar_message(cfg.statusbar_timeout, "key bound");
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
		logmsg(LOG_ERROR, "failed to parse value from command: set %s %s", varname, value);

	/* acquire the value string and print it */
	message = var_value_message(this_var);
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
	message = var_value_message(this_var);
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

void sort_tasks(task *first, task *last) /* {{{ */
{
	/* sort the list of tasks */
	task *start, *cur, *oldcur;

	/* check if we are done */
	if (first==last)
		return;

	/* set start and current */
	start = first;
	cur = start->next;

	/* iterate through to right end, sorting as we go */
	while (1)
	{
		if (compare_tasks(start, cur, cfg.sortmode)==1)
			swap_tasks(start, cur);
		if (cur==last)
			break;
		cur = cur->next;
	}

	/* swap first and cur */
	swap_tasks(first, cur);

	/* save this cur */
	oldcur = cur;

	/* sort left side */
	cur = cur->prev;
	if (cur != NULL)
	{
		if ((first->prev != cur) && (cur->next != first))
			sort_tasks(first, cur);
	}

	/* sort right side */
	cur = oldcur->next;
	if (cur != NULL)
	{
		if ((cur->prev != last) && (last->next != cur))
			sort_tasks(cur, last);
	}
} /* }}} */

void sort_wrapper(task *first) /* {{{ */
{
	/* a wrapper around sort_tasks that finds the last element
	 * to pass to that function
	 */
	task *last;

	/* loop through looking for last item */
	last = first;
	while (last->next != NULL)
		last = last->next;

	/* run sort with last value */
	sort_tasks(first, last);
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
		puts(message);
		logmsg(LOG_DEBUG, "(stdscr==NULL) %s", message);
		free(message);
		return;
	}

	wipe_statusbar();

	/* print message */
	umvaddstr(size[1]-1, 0, message);
	free(message);

	/* set timeout */
	if (dtmout>=0)
		sb_timeout = time(NULL) + dtmout;

	refresh();
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

void swap_tasks(task *a, task *b) /* {{{ */
{
	/* swap the contents of two tasks */
	unsigned short ustmp;
	unsigned int uitmp;
	char *strtmp;
	char ctmp;

	ustmp = a->index;
	a->index = b->index;
	b->index = ustmp;

	strtmp = a->uuid;
	a->uuid = b->uuid;
	b->uuid = strtmp;

	strtmp = a->tags;
	a->tags = b->tags;
	b->tags = strtmp;

	uitmp = a->start;
	a->start = b->start;
	b->start = uitmp;

	uitmp = a->end;
	a->end = b->end;
	b->end = uitmp;

	uitmp = a->entry;
	a->entry = b->entry;
	b->entry = uitmp;

	uitmp = a->due;
	a->due = b->due;
	b->due = uitmp;

	strtmp = a->project;
	a->project = b->project;
	b->project = strtmp;

	ctmp = a->priority;
	a->priority = b->priority;
	b->priority = ctmp;

	strtmp = a->description;
	a->description = b->description;
	b->description = strtmp;
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

	logmsg(LOG_DEBUG, "running: %s", cmd);
	ret = system(cmd);
	free(cmd);
	if (wait)
	{
		puts("press ENTER to return");
		fflush(0);
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
	logmsg(LOG_DEBUG, "running: %s", cmd);
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
	def_shell_mode();
	cmd = malloc(128*sizeof(char));
	if (cfg.version[0]<'2')
		sprintf(cmd, "task edit %d", tasknum);
	else
		sprintf(cmd, "task %d edit", tasknum);
	system(cmd);
	free(cmd);
	reset_shell_mode();
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

static bool task_match(const task *cur, const char *str) /* {{{ */
{
	if (match_string(cur->project, str) ||
			match_string(cur->description, str) ||
			match_string(cur->tags, str))
		return 1;
	else
		return 0;
} /* }}} */

int umvaddstr(const int y, const int x, const char *format, ...) /* {{{ */
{
	/* convert a string to a wchar string and mvaddwstr */
	int len, r;
	wchar_t *wstr;
	char *str;
	va_list args;

	/* build str */
	va_start(args, format);
	const int slen = sizeof(wchar_t)*(size[0]-x+1)/sizeof(char);
	str = calloc(slen, sizeof(char));
	vsnprintf(str, slen-1, format, args);
	va_end(args);

	/* allocate wchar_t string */
	len = strlen(str)+1;
	wstr = calloc(len, sizeof(wchar_t));

	/* check for valid allocation */
	if (wstr==NULL)
	{
		logmsg(LOG_ERROR, "critical: umvaddstr failed to malloc");
		return -1;
	}

	/* perform conversion and write to screen */
	mbstowcs(wstr, str, len);
	len = wcslen(wstr);
	if (len>size[0]-x)
		len = size[0]-x;
	r = mvaddnwstr(y, x, wstr, len);

	/* free memory allocated */
	free(wstr);
	free(str);

	return r;
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

char *var_value_message(var *v) /* {{{ */
{
	/* format a message containing the name and value of a variable */
	char *message;

	switch(v->type)
	{
		case VAR_INT:
			asprintf(&message, "%s: %d", v->name, *(int *)(v->ptr));
			break;
		case VAR_CHAR:
			asprintf(&message, "%s: %c", v->name, *(char *)(v->ptr));
			break;
		case VAR_STR:
			asprintf(&message, "%s: %s", v->name, *(char **)(v->ptr));
			break;
		default:
			asprintf(&message, "variable type unhandled");
			break;
	}

	return message;
} /* }}} */

void wipe_screen(const short startl, const short stopl) /* {{{ */
{
	/* clear specified lines of the screen */
	int y, x;

	attrset(COLOR_PAIR(0));

	for (y=startl; y<=stopl; y++)
		for (x=0; x<size[0]; x++)
			mvaddch(y, x, ' ');
} /* }}} */

int main(int argc, char **argv) /* {{{ */
{
	/* declare variables */
	int c, debug = 0;

	/* open log */
	logfp = fopen(LOGFILE, "a");
	logmsg(LOG_DEBUG, "%s started", SHORTNAME);

	/* set defaults */
	cfg.loglvl = LOG_DEFAULT;
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
		puts("it appears that your task list is empty");
		printf("please add some tasks for %s to manage\n", SHORTNAME);
		return 1;
	}

	/* run ncurses */
	if (!debug)
	{
		logmsg(LOG_DEBUG, "running gui");
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
		char *tmp = var_value_message(find_var("task_version"));
		puts(tmp);
		free(tmp);
		handle_command(test);
		tmp = var_value_message(find_var("task_version"));
		puts(tmp);
		free(tmp);
		free(test);
		asprintf(&tmp, "set search_string tasknc");
		handle_command(tmp);
		test = var_value_message(find_var("search_string"));
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
		cleanup();
	}

	/* done */
	logmsg(LOG_DEBUG, "exiting");
	return 0;
} /* }}} */
