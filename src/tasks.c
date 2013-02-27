/*
 * tasks - handle Taskwarrior io and task structs
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <time.h>
#include "common.h"
#include "config.h"
#include "log.h"
#include "sort.h"
#include "tasklist.h"
#include "tasks.h"

/* local function declarations */
static time_t strtotime(const char *);
static void set_char(char *, char **);
static void set_date(time_t *, char **);
static void set_int(unsigned short *, char **);
static void set_string(char **, char **);

char free_task(task *tsk) /* {{{ */
{
	/* free the memory allocated to a task
	 * tsk - the task to free
	 * return is always 0
	 */
	char ret = 0;

	free(tsk->uuid);
	if (tsk->tags!=NULL)
		free(tsk->tags);
	if (tsk->project!=NULL)
		free(tsk->project);
	if (tsk->description!=NULL)
		free(tsk->description);
	free(tsk);

	return ret;
} /* }}} */

void free_tasks(task *head) /* {{{ */
{
	/* free the task stack
	 * head - the first task on the stack to free
	 */
	task *cur, *next;

	cur = head;
	while (cur!=NULL)
	{
		next = cur->next;
		free_task(cur);
		cur = next;
	}
} /* }}} */

task *get_task_by_position(int n) /* {{{ */
{
	/* get task at line #n
	 * n - line number to retrieve task from
	 * return is the pointer to the task found
	 * or null if n > # of tasks on the stack
	 */
	task *cur;
	short i = -1;

	cur = head;
	while (cur!=NULL)
	{
		i++;
		if (i==n)
			break;
		cur = cur->next;
	}

	return cur;
} /* }}} */

int get_task_position_by_uuid(const char *uuid) /* {{{ */
{
	/* find the task with the specified uuid
	 * uuid - the uuid to match
	 * return is the line number which matches the uuid
	 * or -1 if no task on the stack matches this uuid
	 */
	task *cur;
	int pos = 0;

	cur = head;
	while (cur!=NULL && !str_eq(cur->uuid, uuid))
	{
		cur = cur->next;
		pos++;
	}

	if (cur!= NULL && str_eq(cur->uuid, uuid))
		return pos;
	else
		return -1;
} /* }}} */

task *get_tasks(char *uuid) /* {{{ */
{
	/* parse the task list
	 * uuid - specific task to obtain information for
	 *        pass NULL to get a full task list
	 * return is the task data for a single task, if a uuid was passed
	 * or all tasks, if uuid == NULL
	 */
	FILE *cmd;
	char *line, *tmp, *cmdstr;
	int linelen = TOTALLENGTH;
	unsigned short counter = 0;
	task *last, *new_head;

	/* generate & run command */
	cmdstr = calloc(128, sizeof(char));
	if (cfg.version[0]<'2')
		strcat(cmdstr, "task export.json");
	else
		strcat(cmdstr, "task export");
	if (active_filter!=NULL)
	{
		strcat(cmdstr, " ");
		strcat(cmdstr, active_filter);
	}
	if (uuid!=NULL)
	{
		strcat(cmdstr, " ");
		strcat(cmdstr, uuid);
	}
	tnc_fprintf(logfp, LOG_DEBUG, "reloading tasks (%s)", cmdstr);
	cmd = popen(cmdstr, "r");
	if (cmd == NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "could not execute command: (%s)", cmdstr);
		tnc_fprintf(stdout, LOG_ERROR, "could not execute command: (%s)", cmdstr);
		free(cmdstr);
		return NULL;
	}
	free(cmdstr);

	/* parse output */
	last = NULL;
	new_head = NULL;
	line = calloc(linelen, sizeof(char));
	while (fgets(line, linelen-1, cmd) != NULL || !feof(cmd))
	{
		task *this;

		/* check for longer lines */
		while (strchr(line, '\n')==NULL)
		{
			linelen += TOTALLENGTH;
			line = realloc(line, linelen*sizeof(char));
			tmp = calloc(TOTALLENGTH, sizeof(char));
			if (fgets(tmp, TOTALLENGTH-1, cmd)==NULL)
				break;
			strcat(line, tmp);
			free(tmp);
		}

		/* remove escapes */
		remove_char(line, '\\');

		/* log line */
		tmp = strchr(line, '\n');
		*tmp = 0;
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, line);

		/* parse line */
		this = parse_task(line);
		if (this == NULL)
			return NULL;
		else if (this == (task *)-1)
			continue;
		else if (this->uuid == NULL ||
				this->description == NULL)
			return NULL;

		/* set pointers */
		this->prev = last;

		if (counter==0)
			new_head = this;
		if (counter>0)
			last->next = this;
		last = this;
		counter++;
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "uuid:        %s", this->uuid);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "description: %s", this->description);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "project:     %s", this->project);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "tags:        %s", this->tags);

		/* prepare a new line */
		free(line);
		line = calloc(linelen, sizeof(char));
	}
	free(line);
	pclose(cmd);

	/* sort tasks */
	if (new_head!=NULL)
		sort_wrapper(new_head);

	return new_head;
} /* }}} */

unsigned short get_task_id(char *uuid) /* {{{ */
{
	/* given a task uuid, find its id using a custom report
	 * necessary to do without uuid addressing in task v2
	 * uuid - the task to find the id of
	 * return is the id of the task specified
	 */
	FILE *cmd;
	char line[128], format[128];
	int ret;
	unsigned short id = 0;

	/* generate format to scan for */
	sprintf(format, "%s %%hu", uuid);

	/* run command */
	cmd = popen("task rc.report.all.columns:uuid,id rc.report.all.labels:UUID,id rc.report.all.sort:id- all status:pending rc._forcecolor=no", "r");
	while (fgets(line, sizeof(line)-1, cmd) != NULL)
	{
		ret = sscanf(line, format, &id);
		if (ret>0)
			break;
	}
	pclose(cmd);

	return id;
} /* }}} */

task *malloc_task(void) /* {{{ */
{
	/* allocate memory for a new task
	 * and initialize values where necessary
	 * return is the newly allocated task
	 */
	task *tsk = calloc(1, sizeof(task));
	if (tsk==NULL)
		return NULL;

	tsk->index = 0;
	tsk->uuid = NULL;
	tsk->tags = NULL;
	tsk->start = 0;
	tsk->end = 0;
	tsk->entry = 0;
	tsk->due = 0;
	tsk->project = NULL;
	tsk->priority = 0;
	tsk->description = NULL;
	tsk->next = NULL;
	tsk->prev = NULL;
	tsk->pair = -1;
	tsk->selpair = -1;

	return tsk;
} /* }}} */

task *parse_task(char *line) /* {{{ */
{
	/* parse a line of output from `task export ...`
	 * line - the line to parse
	 * return is the task structure defined in the line,
	 * or -1 if this line is not a task or parsing failed
	 */
	task *tsk = malloc_task();

	/* detect lines that are not json */
	if (*line!='{')
		return (task *) -1;
	line++;

	/* parse json */
	while (*line != 0)
	{
		/* local vars */
		char *field = NULL;
		int ret;

		/* skip field delimiters */
		if (*line == ',')
		{
			line++;
			continue;
		}

		/* terminate at end of line */
		if (*line == '}')
			break;

		/* get field */
		ret = sscanf(line, "\"%m[^\"]\":", &field);
		if (ret != 1)
		{
			tnc_fprintf(logfp, LOG_ERROR, "error parsing task @ %s", line);
			return (task *) -1;
		}
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "field: %s", field);
		line += strlen(field) + 3;

		/* determine how to handle content */
		if (str_eq(field, "id"))
			set_int(&(tsk->index), &line);
		else if (str_eq(field, "description"))
			set_string(&(tsk->description), &line);
		else if (str_eq(field, "project"))
			set_string(&(tsk->project), &line);
		else if (str_eq(field, "tags"))
		{
			char *pos = line;
			while (*pos != ']')
				pos++;
			tsk->tags = strndup(line+1, pos-line-1);
			line = pos+1;
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "string: %s", tsk->tags);
		}
		else if (str_eq(field, "uuid"))
			set_string(&(tsk->uuid), &line);
		else if (str_eq(field, "entry"))
			set_date(&(tsk->entry), &line);
		else if (str_eq(field, "due"))
			set_date(&(tsk->due), &line);
		else if (str_eq(field, "priority"))
			set_char(&(tsk->priority), &line);
		else if (str_eq(field, "annotations"))
		{
			while (*line != ']')
				line++;
			line++;
		}
		free(field);
	}

	return tsk;
} /* }}} */

void reload_task(task *this) /* {{{ */
{
	/* reload an individual task's data
	 * this - the task whose data needs reloading
	 * task data is modified by generating a new task struct, and replacing
	 * the old task in the stack
	 */
	task *new;

	/* get new task */
	new = get_tasks(this->uuid);

	/* check for NULL new task */
	if (new == NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "reload_task(%s): get_tasks returned NULL", this->uuid);
		if (this->prev!=NULL)
			this->prev->next = this->next;
		else
			head = this->next;
		if (this->next!=NULL)
			this->next->prev = this->prev;
		taskcount--;
	}
	else
	{
		/* transfer pointers */
		new->prev = this->prev;
		new->next = this->next;
		if (this->prev!=NULL)
			this->prev->next = new;
		else
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "reload_task(%s): no previous task", this->uuid);
		if (this->next!=NULL)
			this->next->prev = new;
		else
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "reload_task(%s): no next task", this->uuid);

		/* move to head if necessary */
		if (this==head)
		{
			head = new;
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "reload_task(%s): setting task as head", this->uuid);
		}
	}

	/* free old task */
	free_task(this);

	/* re-sort task list */
	sort_wrapper(head);
} /* }}} */

void reload_tasks() /* {{{ */
{
	/* reset head with a new list of tasks */
	task *cur;

	tnc_fprintf(logfp, LOG_DEBUG, "reloading tasks");

	free_tasks(head);

	head = get_tasks(NULL);

	/* debug */
	cur = head;
	while (cur!=NULL)
	{
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "%d,%s,%s,%llu,%llu,%llu,%llu,%s,%c,%s", cur->index, cur->uuid, cur->tags, (unsigned long long)cur->start, (unsigned long long)cur->end, (unsigned long long)cur->entry, (unsigned long long)cur->due, cur->project, cur->priority, cur->description);
		cur = cur->next;
	}
} /* }}} */

void remove_char(char *str, char remove) /* {{{ */
{
	/* iterate through a string and remove escapes inline
	 * str    - the string to remove escapes from
	 * remove - the escape to remove
	 */
	const int len = strlen(str);
	int i, offset = 0;

	for (i=0; i<len; i++)
	{
		if (str[i+offset]=='\0')
			break;
		str[i] = str[i+offset];
		while (str[i]==remove || str[i]=='\0')
		{
			offset++;
			if (i+offset>=len)
				break;
			str[i] = str[i+offset];
		}
		if (str[i+offset]=='\0')
			break;
	}

} /* }}} */

void set_char(char *field, char **line) /* {{{ */
{
	/* set a character field from the next contents of line
	 * field - the field set the character in
	 * line  - the line to parse the character from
	 */
	int ret = sscanf(*line, "\"%c\"", field);
	if (ret != 1)
		tnc_fprintf(logfp, LOG_ERROR, "error parsing char @ %s", *line);
	else
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "char: %c", *field);
	while (**line != ',' && **line != '}')
		(*line)++;
} /* }}} */

void set_date(time_t *field, char **line) /* {{{ */
{
	/* set a time field from the next contents of line
	 * field - the field set the time in
	 * line  - the line to parse the time from
	 */
	char *tmp;
	int ret = sscanf(*line, "\"%m[^\"]\"", &tmp);
	if (ret != 1)
		tnc_fprintf(logfp, LOG_ERROR, "error parsing time @ %s", *line);
	else
	{
		*field = strtotime(tmp);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "time: %d", (int)*field);
		free(tmp);
	}
	while (**line != ',' && **line != '}')
		(*line)++;
} /* }}} */

void set_int(unsigned short *field, char **line) /* {{{ */
{
	/* set an integer field from the next contents of line
	 * field - the field set the integer in
	 * line  - the line to parse the integer from
	 */
	int ret = sscanf(*line, "%hd", field);
	if (ret != 1)
		tnc_fprintf(logfp, LOG_ERROR, "error parsing integer @ %s", *line);
	else
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "int: %d", *field);
	while (**line != ',' && **line != '}')
		(*line)++;
} /* }}} */

void set_string(char **field, char **line) /* {{{ */
{
	/* set an string field from the next contents of line
	 * field - the field set the string in
	 * line  - the line to parse the string from
	 */
	char *tmp = *line;
	do
	{
		tmp = strstr(*line, "\",");
		if (tmp == NULL)
			tmp = strstr(*line, "\"}");
	} while (tmp != NULL && *(tmp-1) == '\\');
	*field = strndup((*line)+1, tmp-(*line)-1);
	if (tmp == NULL || *field == NULL)
		tnc_fprintf(logfp, LOG_ERROR, "error parsing string @ %s", *line);
	else
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "string: %s", *field);
	*line += tmp - *line;
	while (**line != ',' && **line != '}')
		(*line)++;
} /* }}} */

void set_position_by_uuid(const char *uuid) /* {{{ */
{
	/* set the cursor position to a uuid's position
	 * uuid - the uuid of the task to select
	 */
	int pos;

	/* check for null uuid */
	if (uuid == NULL)
		return;

	/* get position & set it */
	pos = get_task_position_by_uuid(uuid);
	if (pos>0)
		selline = pos;
} /* }}} */

time_t strtotime(const char *timestr) /* {{{ */
{
	/* convert a string to a time_t
	 * timestr - the string to parse
	 * return is the time structure parsed
	 */
	struct tm tmr;

	memset(&tmr, 0, sizeof(tmr));
	strptime(timestr, "%Y%m%dT%H%M%S%z", &tmr);
	return mktime(&tmr);
} /* }}} */

int task_background_command(const char *cmdfmt) /* {{{ */
{
	/* run a command on the current task in the background
	 * cmdfmt - the format string describing the command to run
	 *          a %s in the format string will be replaced with
	 *          the selected task's uuid
	 * return is the return of the command run
	 */
	task *cur;
	char *cmdstr, *line;
	FILE *cmd;
	int ret;

	/* build command */
	cur = get_task_by_position(selline);
	asprintf(&cmdstr, cmdfmt, cur->uuid);
	cmdstr = realloc(cmdstr, (strlen(cmdstr)+6)*sizeof(char));
	strcat(cmdstr, " 2>&1");
	tnc_fprintf(logfp, LOG_DEBUG, "running command: %s", cmdstr);

	/* run command in background */
	cmd = popen(cmdstr, "r");
	free(cmdstr);
	while (!feof(cmd))
	{
		ret = fscanf(cmd, "%m[^\n]*", &line);
		if (ret == 1)
		{
			tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, line);
			free(line);
		}
		else
			break;
	}
	ret = pclose(cmd);

	/* log command return value */
	if (WEXITSTATUS(ret) == 0 || WEXITSTATUS(ret) == 128+SIGPIPE)
		ret = 0;
	else
		ret = WEXITSTATUS(ret);
	tnc_fprintf(logfp, LOG_DEBUG, "command returned: %d", ret);

	return ret;
} /* }}} */

void task_count() /* {{{ */
{
	taskcount = 0;
	task *cur;

	cur = head;
	while (cur!=NULL)
	{
		taskcount++;
		cur = cur->next;
	}
} /* }}} */

int task_interactive_command(const char *cmdfmt) /* {{{ */
{
	/* run a command on the current task in the foreground
	 * cmdfmt - the format string describing the command to run
	 *          a %s in the format string will be replaced with
	 *          the selected task's uuid
	 * return is the return of the command run
	 */
	task *cur;
	char *cmdstr;
	int ret;

	/* build command */
	cur = get_task_by_position(selline);
	asprintf(&cmdstr, cmdfmt, cur->uuid);
	tnc_fprintf(logfp, LOG_DEBUG, "running command: %s", cmdstr);

	/* exit window */
	def_prog_mode();
	endwin();

	/* run command */
	ret = system(cmdstr);

	/* log command return value */
	tnc_fprintf(logfp, LOG_DEBUG, "command returned: %d", WEXITSTATUS(ret));

	/* force redraw */
	reset_prog_mode();
	redraw = true;

	return ret;
} /* }}} */

bool task_match(const task *cur, const char *str) /* {{{ */
{
	/* check if specified task meets specified conditions
	 * cur - the task to check
	 * str - the conditions to apply to the task
	 *       refer to the manual for how conditions are specified
	 * return is whether the task matches
	 */
	if (match_string(cur->project, str) ||
			match_string(cur->description, str) ||
			match_string(cur->tags, str))
		return 1;
	else
		return 0;
} /* }}} */

void task_modify(const char *argstr) /* {{{ */
{
	/* run a modify command on the selected task
	 * argstr - the command to run on the selected task
	 *          this will be appended to `task UUID modify `
	 */
	task *cur;
	char *cmd, *uuid;
	int arglen;

	if (argstr!=NULL)
		arglen = strlen(argstr);
	else
		arglen = 0;

	cur = get_task_by_position(selline);
	cmd = calloc(64+arglen, sizeof(char));

	strcpy(cmd, "task %s modify ");
	if (arglen>0)
		strcat(cmd, argstr);

	task_background_command(cmd);

	uuid = strdup(cur->uuid);
	reload_task(cur);
	if (cfg.follow_task)
	{
		set_position_by_uuid(uuid);
		tasklist_check_curs_pos();
	}
	check_free(uuid);

	free(cmd);
} /* }}} */

// vim: noet ts=4 sw=4 sts=4
