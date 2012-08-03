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
#include "color.h"
#include "common.h"
#include "config.h"
#include "keys.h"
#include "log.h"
#include "pager.h"
#include "tasklist.h"
#include "tasknc.h"

/* local functions */
static void pager_window(line *, const bool, int, char *);

/* global variables */
int offset, height, linecount;
bool pager_done;

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

void help_window() /* {{{ */
{
	/* display a help window */
	line *head, *cur, *last;
	keybind *this;
	char *modestr, *keyname;

	head = calloc(1, sizeof(line));
	head->str = strdup("keybinds");

	last = head;
	this = keybinds;
	while (this!=NULL)
	{
		cur = calloc(1, sizeof(line));
		last->next = cur;
		if (this->mode == MODE_TASKLIST)
			modestr = "tasklist";
		else if (this->mode == MODE_PAGER)
			modestr = "pager   ";
		else
			modestr = "unknown ";
		keyname = name_key(this->key);
		if (this->argstr == NULL)
			asprintf(&(cur->str), "%8s\t%3d\t%s\t%s", keyname, this->key, modestr, name_function(this->function));
		else
			asprintf(&(cur->str), "%8s\t%3d\t%s\t%s %s", keyname, this->key, modestr, name_function(this->function), this->argstr);
		free(keyname);
		this = this->next;
		last = cur;
	}

	pager_window(head, 1, -1, " help");
	free_lines(head);
} /* }}} */

void key_pager_close() /* {{{ */
{
	pager_done = true;
} /* }}} */

void key_pager_scroll_up() /* {{{ */
{
	if (offset>0)
		offset--;
	else
		statusbar_message(cfg.statusbar_timeout, "already at top");
} /* }}} */

void key_pager_scroll_down() /* {{{ */
{
	if (offset<linecount+1-height)
		offset++;
	else
		statusbar_message(cfg.statusbar_timeout, "already at bottom");
} /* }}} */

void pager_command(const char *cmdstr, const char *title, const bool fullscreen, const int head_skip, const int tail_skip) /* {{{ */
{
	/* run a command and page through its results */
	FILE *cmd;
	char *str;
	int count = 0, maxlen = 0, len;
	line *head = NULL, *last = NULL, *cur;

	/* run command, gathering strs into a buffer */
	cmd = popen(cmdstr, "r");
	str = calloc(TOTALLENGTH, sizeof(char));
	while (fgets(str, TOTALLENGTH, cmd) != NULL)
	{
		len = strlen(str);

		if (len>maxlen)
			maxlen = len;

		cur = calloc(1, sizeof(line));
		cur->str = str;

		if (count == head_skip)
			head = cur;
		else if (count < head_skip)
		{
			free(cur->str);
			free(cur);
		}
		else if (last != NULL)
			last->next = cur;

		str = calloc(256, sizeof(char));
		count++;
		last = cur;
	}
	free(str);
	pclose(cmd);
	count -= tail_skip;

	/* run pager */
	pager_window(head, fullscreen, count, (char *)title);

	/* free lines */
	free_lines(head);
} /* }}} */

void pager_window(line *head, const bool fullscreen, int nlines, char *title) /* {{{ */
{
	/* page through a series of lines */
	int startx, starty, lineno, c;
	line *tmp;
	offset = 0;
	WINDOW *last_pager = NULL;

	/* store previous pager window if necessary */
	if (pager != NULL)
		last_pager = pager;

	/* count lines if necessary */
	if (nlines<=0)
	{
		tmp = head;
		nlines = 0;
		while (tmp!=NULL)
		{
			nlines++;
			tmp = tmp->next;
		}
	}

	/* exit if there are no lines */
	tnc_fprintf(logfp, LOG_DEBUG, "pager: linecount=%d", linecount);
	if (nlines==0)
		return;
	linecount = nlines;

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

	pager_done = false;
	while (1)
	{
		tnc_fprintf(logfp, LOG_DEBUG, "offset:%d height:%d lines:%d", offset, height, linecount);

		/* print title */
		wattrset(pager, get_colors(OBJECT_HEADER, NULL, NULL));
		mvwhline(pager, 0, 0, ' ', cols);
		umvaddstr_align(pager, 0, title);

		/* print lines */
		wattrset(pager, COLOR_PAIR(0));
		tmp = head;
		lineno = 1;
		while (tmp!=NULL && lineno<=linecount && lineno-offset<=height)
		{
			mvwhline(pager, lineno-offset, 0, ' ', cols);
			if (lineno>offset)
				umvaddstr(pager, lineno-offset, 0, tmp->str);

			lineno++;
			tmp = tmp->next;
		}
		touchwin(pager);
		wrefresh(pager);

		/* accept keys */
		c = wgetch(statusbar);
		handle_keypress(c, MODE_PAGER);

		if (pager_done)
		{
			pager_done = false;
			break;
		}

		statusbar_timeout();
	}

	/* destroy window and force redraw of tasklist */
	delwin(pager);
	if (last_pager == NULL)
	{
		pager = NULL;
		redraw = true;
	}
	else
	{
		tasklist_print_task_list();
		pager = last_pager;
	}
} /* }}} */

void view_stats() /* {{{ */
{
	/* run task stat and page the output */
	char *cmdstr;
	asprintf(&cmdstr, "task stat rc._forcecolor=no rc.defaultwidth=%d", cols-4);
	const char *title = "task statistics";

	/* run pager */
	pager_command(cmdstr, title, 1, 1, 4);

	/* clean up */
	free(cmdstr);
} /* }}} */

void view_task(task *this) /* {{{ */
{
	/* read in task info and print it to a window */
	char *cmdstr, *title;

	/* build command and title */
	asprintf(&cmdstr, "task %s info rc._forcecolor=no rc.defaultwidth=%d", this->uuid, cols-4);
	title = (char *)eval_string(2*cols, cfg.formats.view, this, NULL, 0);

	/* run pager */
	pager_command(cmdstr, title, 0, 1, 4);

	/* clean up */
	free(cmdstr);
	free(title);
} /* }}} */
