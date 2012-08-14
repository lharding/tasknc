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
#include "formats.h"
#include "log.h"
#include "tasks.h"
#include "tasknc.h"
#include "test.h"

#ifdef TASKNC_INCLUDE_TESTS
/* local functions {{{ */
void test_compile_fmt();
void test_eval_string();
void test_result(const char *, const bool);
void test_search();
void test_set_var();
void test_task_count();
void test_trim();
/* }}} */

FILE *devnull;
FILE *out;

void test(const char *args) /* {{{ */
{
	/* run tests to check functionality of tasknc */
	struct test
	{
		const char *name;
		void (*function)();
	};
	struct test tests[] =
	{
		{"compile_fmt", test_compile_fmt},
		{"eval_string", test_eval_string},
		{"task_count", test_task_count},
		{"trim", test_trim},
		{"search", test_search},
		{"set_var", test_set_var},
	};
	const int ntests = sizeof(tests)/sizeof(struct test);
	int i;

	/* get file handles */
	devnull = fopen("/dev/null", "w");
	out = stdout;

	/* determine which tests to run */
	if (str_eq(args, "all"))
	{
		for (i=0; i<ntests; i++)
			(tests[i].function)();
	}
	else
	{
		for (i=0; i<ntests; i++)
		{
			if (strstr(args, tests[i].name)!=NULL)
				(tests[i].function)();
		}
	}

	fclose(devnull);

	cleanup();
} /* }}} */

void test_compile_fmt() /* {{{ */
{
	/* test compiling a format to a series of fields */
	fmt_field **fmts;
	char *eval;

	fmts = compile_string("first $date $-8program_version $4program_name $10program_author second");
	eval = eval_format(fmts, NULL);
	if (eval != NULL)
		printf("%s\n", eval);
	else
		puts("NULL returned");
	fmts = compile_string("first uuid:'$uuid' pro:'$project' desc:'$description' $badvar second");
	eval = eval_format(fmts, head);
	if (eval != NULL)
		printf("%s\n", eval);
	else
		puts("NULL returned");
} /* }}} */

void test_eval_string() /* {{{ */
{
	/* test the functionality of eval_string function */
	char *testfmt = strdup("\"$10program_name - $5program_author ?$search_string?YES?NO?\"");
	strip_quotes(&testfmt, 1);
	const char *eval = eval_string(1000, testfmt, head, NULL, 0);
	const char *eval2 = eval_string(1000, testfmt, head, NULL, 0);
	const char *check = "tasknc     - mjhea NO";
	bool pass = strcmp(check, eval)==0;
	bool pass2 = strcmp(check, eval2)==0;
	pass = pass && pass2;

	test_result("eval_string", pass);
	if (!pass)
	{
		puts(testfmt);
		puts(eval);
		puts(eval2);
		puts(check);
	}

	free(testfmt);
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

void test_search() /* {{{ */
{
	/* test search functionality */
	char *addcmdstr, *testcmdstr, *tmp;
	const char *proj = "test123";
	const char pri = 'H';
	const char *unique = "simple";
	FILE *cmdout;
	task *this;
	bool pass;

	asprintf(&addcmdstr, "task add pro:%s pri:%c %s", proj, pri, unique);
	cmdout = popen(addcmdstr, "r");
	pclose(cmdout);
	free_tasks(head);
	head = get_tasks(NULL);

	stdout = devnull;
	searchstring = strdup(unique);
	find_next_search_result(head, head);
	stdout = out;
	this = get_task_by_position(selline);
	pass = strcmp(this->project, proj)==0 && this->priority==pri;
	test_result("search", pass);
	if (!pass)
	{
		puts(addcmdstr);
		tmp = var_value_message(find_var("search_string"), 1);
		puts(tmp);
		free(tmp);
		puts("selected:");
		printf("uuid: %s\n", this->uuid);
		printf("description: %s\n", this->description);
		printf("project: %s\n", this->project);
		printf("tags: %s\n", this->tags);
		fflush(stdout);
		asprintf(&testcmdstr, "task list %s", unique);
		system(testcmdstr);
		free(testcmdstr);
	}

	cmdout = popen("task undo", "r");
	pclose(cmdout);
	free(addcmdstr);
} /* }}} */

void test_set_var() /* {{{ */
{
	/* test the ability to set a variable */
	char *teststr = strdup("  set \t task_version   0.6.9  ");
	char *testint = strdup("  set \t nc_timeout \t 6969\t\t \n ");

	stdout = devnull;
	handle_command(teststr);
	handle_command(testint);
	stdout = out;
	test_result("set string var", strcmp(cfg.version, "0.6.9")==0);
	test_result("set int var", cfg.nc_timeout==6969);
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

void test_trim() /* {{{ */
{
	/* test the functionality of str_trim */
	bool pass;
	const char *ref = "test string";
	char *check;

	char *teststr = strdup("\n\t test string \t  \n");
	check = str_trim(teststr);
	pass = strcmp(check, ref) == 0;
	test_result("str_trim", pass);
	if (!pass)
	{
		printf("(%d) %s\n", (int)strlen(check), check);
		printf("(%d) %s\n", (int)strlen(ref), ref);
	}
	free(teststr);
} /* }}} */

#else
void test(const char *args) /* {{{ */
{
	strcmp(args, "all");
	puts("test suite not included at compile time");
} /* }}} */

#endif
