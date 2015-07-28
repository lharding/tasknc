/*
 * common.c - generic functions
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "config.h"

/* externs */
extern int selline;

bool match_string(const char *haystack, const char *needle) /* {{{ */
{
	/* find the regex needle in a haystack */
	regex_t regex;
	bool ret;

	/* check for NULL haystack or needle */
	if (haystack==NULL || needle==NULL)
		return false;

	/* compile regex */
	if (regcomp(&regex, needle, REGEX_OPTS) != 0)
		return false;

	/* run regex */
	if (regexec(&regex, haystack, 0, 0, 0) != REG_NOMATCH)
		ret = true;
	else
		ret = false;
	regfree(&regex);
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

char *utc_time(const time_t timeint) /* {{{ */
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
	strftime(timestr, TIMELENGTH, "%H:%M", tmr);

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

// vim: noet ts=4 sw=4 sts=4
