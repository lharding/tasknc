/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * tasks - handle Taskwarrior io and task structs
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <curses.h>
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

char free_task(task *tsk) /* {{{ */
{
	/* free the memory allocated to a task */
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
	/* free the task stack */
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
	/* navigate to line n
	 * and return its task pointer
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
	 * return its line number
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
	/* given a task uuid, find its id
	 * we do this using a custom report
	 * necessary to do without uuid addressing in task v2
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

	return tsk;
} /* }}} */

task *parse_task(char *line) /* {{{ */
{
	task *tsk = malloc_task();
	char *token, *tmpcontent;
	tmpcontent = NULL;
	int tokencounter = 0;

	/* detect lines that are not json */
	if (line[0]!='{')
		return (task *) -1;

	/* parse json */
	token = strtok(line, ",");
	while (token != NULL)
	{
		char *field, *content, *divider, endchar;

		/* increment counter */
		tokencounter++;

		/* determine field name */
		if (token[0] == '{')
			token++;
		if (token[0] == '"')
			token++;
		divider = strchr(token, ':');
		if (divider==NULL)
			break;
		(*divider) = '\0';
		(*(divider-1)) = '\0';
		field = token;

		/* get content */
		content = divider+2;
		if (str_eq(field, "tags") || str_eq(field, "annotations"))
			endchar = ']';
		else if (str_eq(field, "id"))
		{
			sscanf(content-1, "%hd", &(tsk->index));
			token = strtok(NULL, ",");
			continue;
		}
		else
			endchar = '"';

		divider = strchr(content, endchar);
		if (divider!=NULL)
			(*divider) = '\0';
		else /* handle commas */
		{
			tmpcontent = strdup(content);
			do
			{
				token = strtok(NULL, ",");
				tmpcontent = realloc(tmpcontent, (strlen(tmpcontent)+strlen(token)+5)*sizeof(char));
				strcat(tmpcontent, ",");
				strcat(tmpcontent, token);
				divider = strchr(tmpcontent, endchar);
			} while (divider==NULL);
			(*divider) = '\0';
			content = tmpcontent;
		}

		/* log content */
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "field: %-11s content: %s", field, content);

		/* handle data */
		if (str_eq(field, "uuid"))
			tsk->uuid = strdup(content);
		else if (str_eq(field, "project"))
			tsk->project = strdup(content);
		else if (str_eq(field, "description"))
			tsk->description = strdup(content);
		else if (str_eq(field, "priority"))
			tsk->priority = content[0];
		else if (str_eq(field, "due"))
			tsk->due = strtotime(content);
		else if (str_eq(field, "tags"))
			tsk->tags = strdup(content);

		/* free tmpcontent if necessary */
		if (tmpcontent!=NULL)
		{
			free(tmpcontent);
			tmpcontent = NULL;
		}

		/* move to the next token */
		token = strtok(NULL, ",");
	}

	if (tokencounter<2)
	{
		free_tasks(tsk);
		return NULL;
	}
	else
		return tsk;
} /* }}} */

void reload_task(task *this) /* {{{ */
{
	/* reload an individual task's data by number */
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
	/* iterate through a string and remove escapes inline */
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

void set_position_by_uuid(const char *uuid) /* {{{ */
{
	/* set the cursor position to a uuid's position */
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
	/* convert a string to a time_t */
	struct tm tmr;

	memset(&tmr, 0, sizeof(tmr));
	strptime(timestr, "%Y%m%dT%H%M%S%z", &tmr);
	return mktime(&tmr);
} /* }}} */

int task_background_command(const char *cmdfmt) /* {{{ */
{
	/* run a command on the current task in the background */
	task *cur;
	char *cmdstr;
	FILE *cmd;
	int ret;

	/* build command */
	cur = get_task_by_position(selline);
	asprintf(&cmdstr, cmdfmt, cur->uuid);
	tnc_fprintf(logfp, LOG_DEBUG, "running command: %s", cmdstr);

	/* run command in background */
	cmd = popen(cmdstr, "r");
	ret = pclose(cmd);

	return WEXITSTATUS(ret);
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
	/* run a command on the current task in the foreground */
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

	/* force redraw */
	reset_prog_mode();
	redraw = true;

	return ret;
} /* }}} */

bool task_match(const task *cur, const char *str) /* {{{ */
{
	if (match_string(cur->project, str) ||
			match_string(cur->description, str) ||
			match_string(cur->tags, str))
		return 1;
	else
		return 0;
} /* }}} */

void task_modify(const char *argstr) /* {{{ */
{
	/* run a modify command on the selected task */
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
