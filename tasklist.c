/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * tasklist.c - tasklist window
 * for tasknc
 * by mjheagle
 */

#define _XOPEN_SOURCE

#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "config.h"
#include "log.h"
#include "tasklist.h"
#include "tasknc.h"
#include "tasks.h"
#include "pager.h"

void key_tasklist_add() /* {{{ */
{
	/* handle a keyboard direction to add new task */
	tasklist_task_add();
	reload = 1;
	statusbar_message(cfg.statusbar_timeout, "task added");
} /* }}} */

void key_tasklist_complete() /* {{{ */
{
	/* complete selected task */
	task *cur = get_task_by_position(selline);
	int ret;

	statusbar_message(cfg.statusbar_timeout, "completing task");

	ret = task_background_command("task %s done");
	tasklist_remove_task(cur);

	if (ret)
		statusbar_message(cfg.statusbar_timeout, "complete failed");
	else
		statusbar_message(cfg.statusbar_timeout, "complete successful");
} /* }}} */

void key_tasklist_delete() /* {{{ */
{
	/* complete selected task */
	task *cur = get_task_by_position(selline);
	int ret;

	statusbar_message(cfg.statusbar_timeout, "deleting task");

	ret = task_background_command("task %s delete");
	tasklist_remove_task(cur);

	if (ret)
		statusbar_message(cfg.statusbar_timeout, "delete failed");
	else
		statusbar_message(cfg.statusbar_timeout, "delete successful");
} /* }}} */

void key_tasklist_edit() /* {{{ */
{
	/* edit selected task */
	task *cur = get_task_by_position(selline);
	int ret;

	statusbar_message(cfg.statusbar_timeout, "editing task");

	ret = task_interactive_command("task %s edit");
	reload_task(cur);

	if (ret)
		statusbar_message(cfg.statusbar_timeout, "edit failed");
	else
		statusbar_message(cfg.statusbar_timeout, "edit successful");
} /* }}} */

void key_tasklist_filter(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to add a new filter */
	if (arg==NULL)
	{
		statusbar_message(-1, "filter by: ");
		set_curses_mode(NCURSES_MODE_STRING);
		check_free(active_filter);
		active_filter = calloc(2*cols, sizeof(char));
		wgetstr(statusbar, active_filter);
		wipe_statusbar();
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = (int)strlen(arg);
		check_free(active_filter);
		active_filter = calloc(arglen, sizeof(char));
		strncpy(active_filter, arg, arglen);
	}

	statusbar_message(cfg.statusbar_timeout, "filter applied");
	selline = 0;
	reload = 1;
} /* }}} */

void key_tasklist_modify(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to add a new filter */
	char *argstr;

	if (arg==NULL)
	{
		statusbar_message(-1, "modify: ");
		set_curses_mode(NCURSES_MODE_STRING);
		argstr = calloc(2*cols, sizeof(char));
		wgetstr(statusbar, argstr);
		wipe_statusbar();
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = (int)strlen(arg);
		argstr = calloc(arglen+1, sizeof(char));
		strncpy(argstr, arg, arglen);
	}

	task_modify(argstr);
	free(argstr);

	statusbar_message(cfg.statusbar_timeout, "task modified");
	redraw = 1;
	sort_wrapper(head);
} /* }}} */

void key_tasklist_reload() /* {{{ */
{
	/* wrapper function to handle keyboard instruction to reload task list */
	reload = 1;
	statusbar_message(cfg.statusbar_timeout, "task list reloaded");
} /* }}} */

void key_tasklist_scroll(const int direction) /* {{{ */
{
	/* handle a keyboard direction to scroll */
	const char oldsel = selline;
	const char oldoffset = pageoffset;

	switch (direction)
	{
		case 'u':
			/* scroll one up */
			if (selline>0)
			{
				selline--;
				if (selline<pageoffset)
					pageoffset--;
			}
			break;
		case 'd':
			/* scroll one down */
			if (selline<taskcount-1)
			{
				selline++;
				if (selline>=pageoffset+rows-2)
					pageoffset++;
			}
			break;
		case 'h':
			/* go to first entry */
			pageoffset = 0;
			selline = 0;
			break;
		case 'e':
			/* go to last entry */
			if (taskcount>rows-2)
				pageoffset = taskcount-rows+2;
			selline = taskcount-1;
			break;
		default:
			statusbar_message(cfg.statusbar_timeout, "invalid scroll direction");
			break;
	}
	if (pageoffset!=oldoffset)
		redraw = 1;
	else
	{
		tasklist_print_task(oldsel, NULL);
		tasklist_print_task(selline, NULL);
	}
	print_header();
} /* }}} */

void key_tasklist_scroll_down() /* {{{ */
{
	key_tasklist_scroll('d');
} /* }}} */

void key_tasklist_scroll_end() /* {{{ */
{
	key_tasklist_scroll('e');
} /* }}} */

void key_tasklist_scroll_home() /* {{{ */
{
	key_tasklist_scroll('h');
} /* }}} */

void key_tasklist_scroll_up() /* {{{ */
{
	key_tasklist_scroll('u');
} /* }}} */

void key_tasklist_search(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to search */
	check_free(searchstring);
	if (arg==NULL)
	{
		statusbar_message(-1, "search phrase: ");
		set_curses_mode(NCURSES_MODE_STRING);

		/* store search string  */
		searchstring = malloc((cols-16)*sizeof(char));
		wgetstr(statusbar, searchstring);
		sb_timeout = time(NULL) + 3;
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
	{
		const int arglen = strlen(arg);
		searchstring = calloc(arglen, sizeof(char));
		strncpy(searchstring, arg, arglen);
	}

	/* go to first result */
	find_next_search_result(head, get_task_by_position(selline));
	check_curs_pos();
	redraw = 1;
} /* }}} */

void key_tasklist_search_next() /* {{{ */
{
	/* handle a keyboard direction to move to next search result */
	if (searchstring!=NULL)
	{
		find_next_search_result(head, get_task_by_position(selline));
		check_curs_pos();
		redraw = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "no active search string");
} /* }}} */

void key_tasklist_sort(const char *arg) /* {{{ */
{
	/* handle a keyboard direction to sort */
	char m;

	/* get sort mode */
	if (arg==NULL)
	{
		statusbar_message(cfg.statusbar_timeout, "enter sort mode: iNdex, Project, Due, pRiority");
		set_curses_mode(NCURSES_MODE_STD_BLOCKING);
		wrefresh(statusbar);

		m = wgetch(statusbar);
		set_curses_mode(NCURSES_MODE_STD);
	}
	else
		m = *arg;

	/* do sort */
	switch (m)
	{
		case 'n':
		case 'p':
		case 'd':
		case 'r':
			cfg.sortmode = m;
			sort_wrapper(head);
			break;
		case 'N':
		case 'P':
		case 'D':
		case 'R':
			cfg.sortmode = m+32;
			sort_wrapper(head);
			break;
		default:
			statusbar_message(cfg.statusbar_timeout, "invalid sort mode");
			break;
	}
	redraw = 1;
} /* }}} */

void key_tasklist_sync() /* {{{ */
{
	/* handle a keyboard direction to sync */
	int ret;

	statusbar_message(cfg.statusbar_timeout, "synchronizing tasks...");

	ret = task_interactive_command("yes n | task merge && task push");

	if (ret==0)
	{
		statusbar_message(cfg.statusbar_timeout, "tasks synchronized");
		reload = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "task syncronization failed");
} /* }}} */

void key_tasklist_undo() /* {{{ */
{
	/* handle a keyboard direction to run an undo */
	int ret;

	ret = task_background_command("task undo");

	if (ret==0)
	{
		statusbar_message(cfg.statusbar_timeout, "undo executed");
		reload = 1;
	}
	else
		statusbar_message(cfg.statusbar_timeout, "undo execution failed (%d)", ret);
} /* }}} */

void key_tasklist_view() /* {{{ */
{
	/* run task info on a task and display in pager */
	view_task(get_task_by_position(selline));
} /* }}} */

void tasklist_window() /* {{{ */
{
	/* ncurses main function */
	int c, oldrows, oldcols;
	cfg.fieldlengths.project = max_project_length();
	cfg.fieldlengths.date = DATELENGTH;

	/* create windows */
	rows = LINES;
	cols = COLS;
	tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "rows: %d, columns: %d", rows, cols);
	header = newwin(1, cols, 0, 0);
	tasklist = newwin(rows-2, cols, 1, 0);
	statusbar = newwin(1, cols, rows-1, 0);
	if (statusbar==NULL)
		tnc_fprintf(logfp, LOG_ERROR, "statusbar creation failed (rows:%d, cols:%d)", rows, cols);

	/* set curses settings */
	set_curses_mode(NCURSES_MODE_STD);

	/* print task list */
	check_screen_size();
	oldrows = LINES;
	oldcols = COLS;
	cfg.fieldlengths.description = oldcols-cfg.fieldlengths.project-1-cfg.fieldlengths.date;
	task_count();
	print_header();
	tasklist_print_task_list();

	/* main loop */
	while (1)
	{
		/* set variables for determining actions */
		done = 0;
		redraw = 0;
		reload = 0;

		/* get the screen size */
		rows = LINES;
		cols = COLS;

		/* check for a screen thats too small */
		check_screen_size();

		/* check for size changes */
		if (cols!=oldcols || rows!=oldrows)
		{
			resize = 1;
			redraw = 1;
			wipe_statusbar();
		}
		oldcols = cols;
		oldrows = rows;

		/* apply staged window updates */
		doupdate();

		/* get a character */
		c = wgetch(statusbar);

		/* handle the character */
		handle_keypress(c, MODE_TASKLIST);

		/* exit */
		if (done==1)
			break;
		/* reload task list */
		if (reload==1)
		{
			wipe_tasklist();
			reload_tasks();
			task_count();
			redraw = 1;
		}
		/* resize windows */
		if (resize==1)
		{
			wresize(header, 1, cols);
			wresize(tasklist, rows-2, cols);
			wresize(statusbar, 1, cols);
			mvwin(header, 0, 0);
			mvwin(tasklist, 1, 0);
			mvwin(statusbar, rows-1, 0);
		}
		/* redraw all windows */
		if (redraw==1)
		{
			cfg.fieldlengths.project = max_project_length();
			cfg.fieldlengths.description = cols-cfg.fieldlengths.project-1-cfg.fieldlengths.date;
			print_header();
			tasklist_print_task_list();
			check_curs_pos();
			wnoutrefresh(tasklist);
			wnoutrefresh(header);
			wnoutrefresh(statusbar);
			doupdate();
		}

		statusbar_timeout();
	}
} /* }}} */

void tasklist_print_task(int tasknum, task *this) /* {{{ */
{
	/* print a task specified by number */
	bool sel = 0;
	char *tmp;
	int x, y;

	/* determine position to print */
	y = tasknum-pageoffset;
	if (y<0 || y>=rows-1)
		return;

	/* find task pointer if necessary */
	if (this==NULL)
		this = get_task_by_position(tasknum);

	/* check if this is NULL */
	if (this==NULL)
	{
		tnc_fprintf(logfp, LOG_ERROR, "task %d is null", tasknum);
		return;
	}

	/* determine if line is selected */
	if (tasknum==selline)
		sel = 1;

	/* wipe line */
	wattrset(tasklist, COLOR_PAIR(0));
	for (x=0; x<cols; x++)
		mvwaddch(tasklist, y, x, ' ');

	/* evaluate line */
	wmove(tasklist, 0, 0);
	wattrset(tasklist, COLOR_PAIR(2+sel));
	tmp = (char *)eval_string(2*cols, cfg.formats.task, this, NULL, 0);
	umvaddstr_align(tasklist, y, tmp);
	free(tmp);

	wnoutrefresh(tasklist);
} /* }}} */

void tasklist_print_task_list() /* {{{ */
{
	/* print every task in the task list */
	task *cur;
	short counter = 0;

	cur = head;
	while (cur!=NULL)
	{
		tasklist_print_task(counter, cur);

		/* move to next item */
		counter++;
		cur = cur->next;
	}
	if (counter<cols-2)
		wipe_screen(tasklist, counter-pageoffset, rows-2);
} /* }}} */

void tasklist_remove_task(task *this) /* {{{ */
{
	/* remove a task from the task list without reloading */
	if (this==head)
		head = this->next;
	else
		this->prev->next = this->next;
	if (this->next!=NULL)
		this->next->prev = this->prev;
	free_task(this);
	taskcount--;
	redraw = 1;
} /* }}} */

void tasklist_task_add(void) /* {{{ */
{
	/* create a new task by adding a generic task
	 * then letting the user edit it
	 */
	FILE *cmdout;
	char *cmd, line[TOTALLENGTH], *redir;
	const char addstr[] = "Created task ";
	unsigned short tasknum;

	/* determine whether stdio should be used */
	if (cfg.silent_shell)
	{
		statusbar_message(cfg.statusbar_timeout, "running task add");
		redir = "> /dev/null";
	}
	else
		redir = "";

	/* add new task */
	cmd = malloc(128*sizeof(char));
	sprintf(cmd, "task add new task %s", redir);
	tnc_fprintf(logfp, LOG_DEBUG, "running: %s", cmd);
	cmdout = popen("task add new task", "r");
	while (fgets(line, sizeof(line)-1, cmdout) != NULL)
	{
		if (strncmp(line, addstr, strlen(addstr))==0)
			if (sscanf(line, "Created task %hu.", &tasknum))
				break;
	}
	pclose(cmdout);
	free(cmd);

	/* edit task */
	def_prog_mode();
	cmd = malloc(128*sizeof(char));
	if (cfg.version[0]<'2')
		sprintf(cmd, "task edit %d", tasknum);
	else
		sprintf(cmd, "task %d edit", tasknum);
	endwin();
	system(cmd);
	free(cmd);
	reset_prog_mode();
	wnoutrefresh(tasklist);
	wnoutrefresh(header);
	wnoutrefresh(statusbar);
} /* }}} */
