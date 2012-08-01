/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * test.c - testing for tasknc
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include "common.h"
#include "config.h"
#include "log.h"
#include "tasks.h"
#include "tasknc.h"

#ifdef TASKNC_INCLUDE_TESTS
/* local functions {{{ */
void test_result(const char *, const bool);
void test_set_var();
void test_task_count();
/* }}} */

void test(const char *tests) /* {{{ */
{
	/* run tests to check functionality of tasknc */

	/* determine which tests to run */
	if (str_eq(tests, "all"))
	{
		test_task_count();
		test_set_var();
	}

	/* char *test; */
	/* asprintf(&test, "set task_version 2.1"); */
	/* char *tmp = var_value_message(find_var("task_version"), 1); */
	/* puts(tmp); */
	/* free(tmp); */
	/* handle_command(test); */
	/* tmp = var_value_message(find_var("task_version"), 1); */
	/* puts(tmp); */
	/* free(tmp); */
	/* free(test); */
	/* asprintf(&tmp, "set search_string tasknc"); */
	/* handle_command(tmp); */
	/* test = var_value_message(find_var("search_string"), 1); */
	/* puts(test); */
	/* free(test); */
	/* printf("selline: %d\n", selline); */
	/* find_next_search_result(head, head); */
	/* printf("selline: %d\n", selline); */
	/* task *t = get_task_by_position(selline); */
	/* if (t==NULL) */
		/* puts("???"); */
	/* else */
		/* puts(t->description); */
	/* free(tmp); */
	/* asprintf(&tmp, "  test string  "); */
	/* printf("%s (%d)\n", tmp, (int)strlen(tmp)); */
	/* test = str_trim(tmp); */
	/* printf("%s (%d)\n", test, (int)strlen(test)); */
	/* free(tmp); */

	/* [> evaluating a format string <] */
	/* char *titlefmt = "$10program_name;();$program_version; $5filter_string//$task_count\\/$badvar"; */
	/* printf("%s\n", eval_string(100, titlefmt, NULL, NULL, 0)); */

	cleanup();
} /* }}} */

void test_result(const char *testname, const bool passed) /* {{{ */
{
	/* print a colored result for a test */
	char *color, *msg;

	/* determine color and message */
	if (passed)
	{
		msg = "passed";
		color = "\033[0;32m";
	}
	else
	{
		msg = "failed";
		color = "\033[0;31m";
	}

	/* print result */
	printf("%-15s %s%s%s\n", testname, color, msg, "\033[0;m");
} /* }}} */

void test_set_var() /* {{{ */
{
	/* test the ability to set a variable */
	handle_command("  set   task_version   0.6.9");
	test_result("set var", strcmp(cfg.version, "0.6.9")==0);
} /* }}} */

void test_task_count() /* {{{ */
{
	/* check that the tasks are counted correctly */
	int tcnt;
	FILE *cmdout;
	char *line, *cmdstr;

	task_count();

	line = calloc(8, sizeof(char));
	asprintf(&cmdstr, "task %s count", active_filter);
	cmdout = popen(cmdstr, "r");
	fgets(line, 8, cmdout);
	pclose(cmdout);
	tcnt = atoi(line);

	test_result("task count", tcnt == taskcount);

	free(line);
} /* }}} */

#else
void test(const char *args) /* {{{ */
{
	strcmp(args, "all");
	puts("test suite not included at compile time");
} /* }}} */

#endif
