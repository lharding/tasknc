/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * common.h
 * for tasknc
 * by mjheagle
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <stdbool.h>

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

/* var struct management */
typedef enum {
	VAR_UNDEF = 0,
	VAR_CHAR,
	VAR_STR,
	VAR_INT
} var_type;

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

/* log levels */
typedef enum {
	LOG_DEFAULT = 0,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_DEBUG_VERBOSE
} log_mode;

/* runtime config */
typedef struct _config {
	int nc_timeout;
	int statusbar_timeout;
	log_mode loglvl;
	char *version;
	char sortmode;
	char silent_shell;
	struct {
		char *task;
                char *title;
	} formats;
	struct {
                int description;
                int date;
                int project;
	} fieldlengths;
} config;

/* string comparison */
#define str_starts_with(x, y)           (strncmp((x),(y),strlen(y)) == 0) 
#define str_eq(x, y)                    (strcmp((x), (y))==0)
#define check_free(x)                   if (x!=NULL) free(x);

/* regex options */
#define REGEX_OPTS REG_ICASE|REG_EXTENDED|REG_NOSUB|REG_NEWLINE

/* functions */
bool match_string(const char *, const char *);

#endif
