/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * common.c - generic functions
 * for tasknc
 * by mjheagle
 */

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"

bool match_string(const char *haystack, const char *needle) /* {{{ */
{
	/* match a string to a regex */
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
