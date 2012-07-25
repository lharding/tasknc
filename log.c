/*
 * log.c
 * for tasknc
 * by mjheagle
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include "log.h"


void tnc_fprintf(FILE *fp, const log_mode minloglvl, const char *format, ...) /* {{{ */
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
	if (fp!=stdout)
		fprintf(fp, "[%s] ", timestr);

	/* log type header */
	switch(minloglvl)
	{
		case LOG_WARN:
			fputs("WARNING: ", fp);
			break;
		case LOG_ERROR:
			fputs("ERROR: ", fp);
			break;
		case LOG_DEBUG:
		case LOG_DEBUG_VERBOSE:
			fputs("DEBUG: ", fp);
		default:
			break;
	}

	/* write log entry */
	va_start(args, format);
	vfprintf(fp, format, args);
	va_end(args);

	/* trailing newline */
	fputc('\n', fp);
} /* }}} */
