/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * view.c - view task info
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "log.h"
#include "pager.h"
#include "tasknc.h"

/* local functions */
static void pager_window(line *, const bool, int, char *);

void free_lines(line *head) /* {{{ */
{
	/* iterate through lines list and free all elements */
	line *cur, *last;

	cur = head;
	while (cur!=NULL)
	{
		last = cur;
		cur = cur->next;
		free(last->str);
		free(last);
	}
} /* }}} */

void pager_window(line *head, const bool fullscreen, int linecount, char *title) /* {{{ */
{
	/* page through a series of lines */
	WINDOW *pager;
	int startx, starty, height, offset = 0, lineno = 1;
	line *tmp;

	/* count lines if necessary */
	if (linecount<=0)
	{
		tmp = head;
		linecount = 0;
		while (tmp!=NULL)
		{
			linecount++;
			tmp = tmp->next;
		}
	}

	/* exit if there are no lines */
	tnc_fprintf(logfp, LOG_DEBUG, "pager: linecount=%d", linecount);
	if (linecount==0)
		return;

	/* determine screen dimensions and create window */
	if (fullscreen)
	{
		height = LINES-2;
		starty = 1;
		startx = 0;
	}
	else
	{
		height = linecount+1;
		starty = rows-height-1;
		startx = 0;
	}
	tnc_fprintf(logfp, LOG_DEBUG, "pager: h=%d w=%d y=%d x=%d", height, cols, starty, startx);
	pager = newwin(height, cols, starty, startx);

	/* print title */
	wattrset(pager, COLOR_PAIR(1));
	mvwhline(pager, 0, 0, ' ', cols);
	umvaddstr_align(pager, 0, title);

	/* print lines */
	wattrset(pager, COLOR_PAIR(0));
	tmp = head;
	while (tmp!=NULL && lineno<=linecount && lineno-offset<=height)
	{
		if (lineno>offset)
			umvaddstr(pager, lineno-offset, 0, tmp->str);

		lineno++;
		tmp = tmp->next;
	}
	touchwin(pager);
	wrefresh(pager);

	/* TODO: accept keys */
	wgetch(pager);

	/* destroy window and force redraw of tasklist */
	delwin(pager);
	redraw = 1;
} /* }}} */

void view_task(task *this) /* {{{ */
{
	/* read in task info and print it to a window */
	FILE *cmd;
	char *str, *cmdstr, *title;
	int count = 0, maxlen = 0, len;
	line *head = NULL, *last = NULL, *cur;

	/* build and run command, gathering strs into a buffer */
	asprintf(&cmdstr, "task %s info rc._forcecolor=no rc.defaultwidth=%d", this->uuid, cols-4);
	cmd = popen(cmdstr, "r");
	free(cmdstr);
	str = calloc(256, sizeof(char));
	while (fgets(str, 256, cmd) != NULL)
	{
		len = strlen(str);

		if (len>maxlen)
			maxlen = len;

		cur = calloc(1, sizeof(line));
		cur->str = str;

		if (count == 1)
			head = cur;

		if (last != NULL)
			last->next = cur;

		str = malloc(256*sizeof(char));
		count++;
		last = cur;
	}
	pclose(cmd);
	count -= 4;

	/* run pager */
	title = (char *)eval_string(2*cols, cfg.formats.view, this, NULL, 0);
	pager_window(head, 0, count, title);
	free(title);

	/* free lines */
	free_lines(head);
} /* }}} */
