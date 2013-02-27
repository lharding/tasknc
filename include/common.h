/*
 * common.h
 * for tasknc
 * by mjheagle
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <stdbool.h>
#include <time.h>

/* ncurses settings */
typedef enum { NCURSES_MODE_STD, NCURSES_MODE_STD_BLOCKING,
	NCURSES_MODE_STRING} ncurses_mode;

/* var type */
typedef enum { VAR_UNDEF, VAR_CHAR, VAR_STR, VAR_INT } var_type;

/* variable permissions */
typedef enum { VAR_RW, VAR_RC, VAR_RO } var_perms;

/**
 * variable container struct
 * name  - the name of the variable
 * type  - the type of variable
 * perms - the permissions of the variable
 * ptr   - a pointer to the variable
 */
typedef struct _var
{
	char *name;
	var_type type;
	var_perms perms;
	void *ptr;
} var;

/**
 * task struct - the main structure in this program!
 * the fields thru description are data from the taskwarrior json
 * selpair - the cached color pair to be used when this task is selected
 * pair    - the cached color pair to be used when this task is not selected
 * prev    - the previous task struct
 * next    - the next task struct
 */
typedef struct _task
{
	/* taskwarrior data */
	unsigned short index;
	char *uuid;
	char *tags;
	time_t start;
	time_t end;
	time_t entry;
	time_t due;
	char *project;
	char priority;
	char *description;
	/* color caching */
	int selpair;
	int pair;
	/* linked list pointers */
	struct _task *prev;
	struct _task *next;
} task;

/* program modes */
typedef enum { MODE_TASKLIST, MODE_PAGER, MODE_ANY } prog_mode;

/* format fields */
typedef enum { FIELD_DATE, FIELD_PROJECT, FIELD_DESCRIPTION, FIELD_DUE,
	FIELD_PRIORITY, FIELD_UUID, FIELD_INDEX, FIELD_STRING, FIELD_VAR,
	FIELD_CONDITIONAL } fmt_field_type;

/**
 * format field struct - for describing portions of format strings
 * type        - the field type which is to be printed
 * variable    - variable struct
 * field       - raw string data
 * conditional - a structure describing a conditional
 * length      - the length of the data contained
 * width       - the width that the field will be padded or cut to
 * right_align - whether the field should be right aligned
 * next        - the next format field struct
 */
typedef struct _fmt_field
{
	fmt_field_type type;
	var *variable;
	char *field;
	struct _conditional_fmt_field *conditional;
	unsigned int length;
	unsigned int width;
	bool right_align;
	struct _fmt_field *next;
} fmt_field;

/**
 * conditional format field struct
 * condition - the condition to be evaluated
 * positive  - the string to be printed if condition was true
 * negative  - the string to be printed if condition was false
 */
typedef struct _conditional_fmt_field
{
	fmt_field *condition;
	fmt_field *positive;
	fmt_field *negative;
} conditional_fmt_field;


/**
 * function maps - map between a function name, mode and pointer
 * name     - the name of the function
 * function - a pointer to the function
 * argn     - the number of arguments the function takes
 * mode     - the mode that this function should be run in
 */
typedef struct _funcmap
{
	char *name;
	void (*function)();
	int argn;
	prog_mode mode;
} funcmap;

/* log levels */
typedef enum { LOG_DEFAULT, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG,
	LOG_DEBUG_VERBOSE } log_mode;

/**
 * runtime configuration struct
 * history_max       - the max number of history items stored (used in prompt history)
 * nc_timeout        - the time ncurses will wait for a keypress in ms
 * statusbar_timeout - the time a statusbar message will display before timing out
 * loglvl            - which log messages should be printed
 * version           - the task warrior version being wrapped
 * sortmode          - the active sort mode
 * follow_task       - whether a task will be followed when it moves in the list
 * formats           - string and compiled printing formats
 * fieldlengths      - width of some task data fields
 */
typedef struct _config {
	int history_max;
	int nc_timeout;
	int statusbar_timeout;
	log_mode loglvl;
	char *version;
	char *sortmode;
	bool follow_task;
	struct {
		char *task;
		fmt_field *task_compiled;
		char *title;
		fmt_field *title_compiled;
		char *view;
		fmt_field *view_compiled;
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

/* logical xor */
#define XOR(x, y)                       ((x || y) && !(x && y))

/* min function */
#define MIN(x, y)                       (x < y ? x : y)

/* functions */
bool match_string(const char *, const char *);
char *utc_date(const time_t);
char *var_value_message(var *, bool);

#endif

// vim: noet ts=4 sw=4 sts=4
