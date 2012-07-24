/*
 * header for tasknc
 * by mjheagle
 */

#ifndef _TASKNC_H
#define _TASKNC_H

/* wiping functions */
#define wipe_tasklist()                 wipe_screen(1, size[1]-2)
#define wipe_statusbar()                wipe_screen(size[1]-1, size[1]-1)

/* string comparison */
#define str_starts_with(x, y)           (strncmp((x),(y),strlen(y)) == 0) 
#define str_eq(x, y)                    (strcmp((x), (y))==0)
#define check_free(x)                   if (x!=NULL) free(x);

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
#define LOGLVL_DEFAULT                  3

#endif
