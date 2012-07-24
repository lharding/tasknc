/*
 * common.h
 * for tasknc
 * by mjheagle
 */

#ifndef _COMMON_H
#define _COMMON_H

/* log levels */
typedef enum {
	LOG_DEFAULT = 0,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
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
} config;

#endif
