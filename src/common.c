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
#include "common.h"

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
