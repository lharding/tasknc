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
typedef enum {
	NCURSES_MODE_STD = 0,
	NCURSES_MODE_STD_BLOCKING,
	NCURSES_MODE_STRING
} ncurses_mode;

/* var type */
typedef enum {
	VAR_UNDEF = 0,
	VAR_CHAR,
	VAR_STR,
	VAR_INT
} var_type;

/* variable permissions */
typedef enum {
	VAR_RW = 0,
	VAR_RC,
	VAR_RO
} var_perms;

/* variable management struct */
typedef struct _var
{
	char *name;
	var_type type;
	var_perms perms;
	void *ptr;
} var;

/* task definition */
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
typedef enum {
	MODE_TASKLIST = 0,
	MODE_PAGER,
	MODE_ANY
} prog_mode;

/* format fields */
typedef enum
{
	FIELD_DATE = 0,
	FIELD_PROJECT,
	FIELD_DESCRIPTION,
	FIELD_DUE,
	FIELD_PRIORITY,
	FIELD_UUID,
	FIELD_INDEX,
	FIELD_STRING,
	FIELD_VAR,
	FIELD_CONDITIONAL
} fmt_field_type;

/* format field struct */
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

/* conditional form field struct */
typedef struct _conditional_fmt_field
{
	fmt_field *condition;
	fmt_field *positive;
	fmt_field *negative;
} conditional_fmt_field;


/* function maps */
typedef struct _funcmap
{
	char *name;
	void (*function)();
	int argn;
	prog_mode mode;
} funcmap;

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
