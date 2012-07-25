/*
 * common.c - generic functions
 * for tasknc
 * by mjheagle
 *
 * vim: noet ts=4 sw=4 sts=4
 */

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include "common.h"

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
