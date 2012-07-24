/*
 * tasks - handle taskwarrior io and task structs
 * for tasknc
 * by mjheagle
 */

#define _XOPEN_SOURCE
#include <time.h>
#include "tasks.h"
#include "config.h"
#include "log.h"

/* local function declarations {{{ */
static char compare_tasks(const task *, const task *, const char);
static void remove_char(char *, char);
static void sort_tasks(task *, task *);
static void swap_tasks(task *, task*);
/* }}} */

char compare_tasks(const task *a, const task *b, const char sort_mode) /* {{{ */
{
	/* compare two tasks to determine order
	 * a return of 1 means that the tasks should be swapped (b comes before a)
	 */
	char ret = 0;
	int tmp;

	/* determine sort algorithm and apply it */
	switch (sort_mode)
	{
		case 'n':       // sort by index
			if (a->index<b->index)
				ret = 1;
			break;
		default:
		case 'p':       // sort by project name => index
			if (a->project == NULL)
			{
				if (b->project != NULL)
					ret = 1;
				break;
			}
			if (b->project == NULL)
				break;
			tmp = strcmp(a->project, b->project);
			if (tmp<0)
				ret = 1;
			if (tmp==0)
				ret = compare_tasks(a, b, 'n');
			break;
		case 'd':       // sort by due date => priority => project => index
			if (a->due == 0)
			{
				if (b->due == 0)
					ret = compare_tasks(a, b, 'r');
				break;
			}
			if (b->due == 0)
			{
				ret = 1;
				break;
			}
			if (a->due<b->due)
				ret = 1;
			break;
		case 'r':       // sort by priority => project => index
			if (a->priority == 0)
			{
				if (b->priority == 0)
					ret = compare_tasks(a, b, 'p');
				break;
			}
			if (b->priority == 0)
			{
				ret = 1;
				break;
			}
			if (a->priority == b->priority)
			{
				ret = compare_tasks(a, b, 'p');
				break;
			}
			switch (b->priority)
			{
				case 'H':
				default:
					break;
				case 'M':
					if (a->priority=='H')
						ret = 1;
					break;
				case 'L':
					if (a->priority=='M' || a->priority=='H')
						ret = 1;
					break;
			}
			break;
	}

	return ret;
} /* }}} */

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
		strcat(cmdstr, "task export.json status:pending");
	else
		strcat(cmdstr, "task export status:pending");
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
	free(cmdstr);

	/* parse output */
	last = NULL;
	new_head = NULL;
	line = calloc(linelen, sizeof(char));
	while (fgets(line, linelen-1, cmd) != NULL)
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
		this->index = counter;
		this->prev = last;

		if (counter==0)
			new_head = this;
		if (counter>0)
			last->next = this;
		last = this;
		counter++;
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "uuid: %s", this->uuid);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "description: %s", this->description);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "project: %s", this->project);
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "tags: %s", this->tags);

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
	char *token, *tmpstr, *tmpcontent;
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
		struct tm tmr;

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
			tmpcontent = malloc((strlen(content)+1)*sizeof(char));
			strcpy(tmpcontent, content);
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
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "field: %s; content: %s", field, content);

		/* handle data */
		if (str_eq(field, "uuid"))
		{
			tsk->uuid = malloc(UUIDLENGTH*sizeof(char));
			strcpy(tsk->uuid, content);
		}
		else if (str_eq(field, "project"))
		{
			tsk->project = malloc(PROJECTLENGTH*sizeof(char));
			strcpy(tsk->project, content);
		}
		else if (str_eq(field, "description"))
		{
			tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
			strcpy(tsk->description, content);
		}
		else if (str_eq(field, "priority"))
			tsk->priority = content[0];
		else if (str_eq(field, "due"))
		{
			memset(&tmr, 0, sizeof(tmr));
			strptime(content, "%Y%m%dT%H%M%S%z", &tmr);
			tmpstr = malloc(32*sizeof(char));
			strftime(tmpstr, 32, "%s", &tmr);
			sscanf(tmpstr, "%d", &(tsk->due));
			free(tmpstr);
		}
		else if (str_eq(field, "tags"))
		{
			tsk->tags = malloc(TAGSLENGTH*sizeof(char));
			strcpy(tsk->tags, content);
		}

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
		totaltaskcount--;
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
		tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "%d,%s,%s,%d,%d,%d,%d,%s,%c,%s", cur->index, cur->uuid, cur->tags, cur->start, cur->end, cur->entry, cur->due, cur->project, cur->priority, cur->description);
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
			str[i] = str[i+offset];
		}
		if (str[i+offset]=='\0')
			break;
	}

} /* }}} */

void sort_wrapper(task *first) /* {{{ */
{
	/* a wrapper around sort_tasks that finds the last element
	 * to pass to that function
	 */
	task *last;

	/* loop through looking for last item */
	last = first;
	while (last->next != NULL)
		last = last->next;

	/* run sort with last value */
	sort_tasks(first, last);
} /* }}} */

void sort_tasks(task *first, task *last) /* {{{ */
{
	/* sort the list of tasks */
	task *start, *cur, *oldcur;

	/* check if we are done */
	if (first==last)
		return;

	/* set start and current */
	start = first;
	cur = start->next;

	/* iterate through to right end, sorting as we go */
	while (1)
	{
		if (compare_tasks(start, cur, cfg.sortmode)==1)
			swap_tasks(start, cur);
		if (cur==last)
			break;
		cur = cur->next;
	}

	/* swap first and cur */
	swap_tasks(first, cur);

	/* save this cur */
	oldcur = cur;

	/* sort left side */
	cur = cur->prev;
	if (cur != NULL)
	{
		if ((first->prev != cur) && (cur->next != first))
			sort_tasks(first, cur);
	}

	/* sort right side */
	cur = oldcur->next;
	if (cur != NULL)
	{
		if ((cur->prev != last) && (last->next != cur))
			sort_tasks(cur, last);
	}
} /* }}} */

void swap_tasks(task *a, task *b) /* {{{ */
{
	/* swap the contents of two tasks */
	unsigned short ustmp;
	unsigned int uitmp;
	char *strtmp;
	char ctmp;

	ustmp = a->index;
	a->index = b->index;
	b->index = ustmp;

	strtmp = a->uuid;
	a->uuid = b->uuid;
	b->uuid = strtmp;

	strtmp = a->tags;
	a->tags = b->tags;
	b->tags = strtmp;

	uitmp = a->start;
	a->start = b->start;
	b->start = uitmp;

	uitmp = a->end;
	a->end = b->end;
	b->end = uitmp;

	uitmp = a->entry;
	a->entry = b->entry;
	b->entry = uitmp;

	uitmp = a->due;
	a->due = b->due;
	b->due = uitmp;

	strtmp = a->project;
	a->project = b->project;
	b->project = strtmp;

	ctmp = a->priority;
	a->priority = b->priority;
	b->priority = ctmp;

	strtmp = a->description;
	a->description = b->description;
	b->description = strtmp;
} /* }}} */
