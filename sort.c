/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * sort.c - handle sorting of tasks
 * for tasknc
 * by mjheagle
 */

#include <string.h>
#include "common.h"
#include "sort.h"

/* local functions */
static bool compare_tasks(const task *, const task *, const char *);
static void sort_tasks(task *, task *);
static void swap_tasks(task *, task*);

bool compare_tasks(const task *a, const task *b, const char *mode_queue) /* {{{ */
{
	/* compare two tasks to determine order
	 * a return of 1 means that the tasks should be swapped (b comes before a)
	 */
	bool ret = false;
	int tmp;

	/* done if mode is null */
	const char sort_mode = *mode_queue;

	/* determine sort algorithm and apply it */
	switch (sort_mode)
	{
		case 'n':       // sort by index
			if (a->index<b->index)
				ret = true;
			break;
		case 'p':       // sort by project name => uuid
			if (a->project == NULL)
			{
				if (b->project != NULL)
					ret = true;
				break;
			}
			if (b->project == NULL)
				break;
			tmp = strcmp(a->project, b->project);
			if (tmp<0)
				ret = true;
			if (tmp==0)
				ret = compare_tasks(a, b, mode_queue+1);
			break;
		case 'd':       // sort by due date => priority => project => uuid
			if (a->due == 0)
			{
				if (b->due == 0)
					ret = compare_tasks(a, b, mode_queue+1);
				break;
			}
			if (b->due == 0)
			{
				ret = true;
				break;
			}
			if (a->due<b->due)
				ret = true;
			break;
		case 'r':       // sort by priority => project => uuid
			if (a->priority == 0)
			{
				if (b->priority == 0)
					ret = compare_tasks(a, b, mode_queue+1);
				break;
			}
			if (b->priority == 0)
			{
				ret = true;
				break;
			}
			if (a->priority == b->priority)
			{
				ret = compare_tasks(a, b, mode_queue+1);
				break;
			}
			switch (b->priority)
			{
				case 'H':
				default:
					break;
				case 'M':
					if (a->priority=='H')
						ret = true;
					break;
				case 'L':
					if (a->priority=='M' || a->priority=='H')
						ret = true;
					break;
			}
			break;
		case 'u':       // sort by uuid
			if (strcmp(a->uuid, b->uuid)<0)
				ret = true;
			break;
		case 0:
		default:
			return false;
			break;
	}

	return ret;
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
		if (compare_tasks(start, cur, cfg.sortmode))
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
