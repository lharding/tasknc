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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "tasknc.h"
#include "view.h"

void view_task(task *this)
{
	/* read in task info and print it to a window */
	FILE *cmd;
	char *line, **lines, *cmdstr, *tmp;
	int count = 0, i, maxlen = 0, len;

	/* build and run command, gathering lines into a buffer */
	lines = calloc(200, sizeof(char *));
	asprintf(&cmdstr, "task %s info rc._forcecolor=no rc.defaultwidth=%d", this->uuid, cols-4);
	cmd = popen(cmdstr, "r");
	free(cmdstr);
	line = malloc(256*sizeof(char));
	while (fgets(line, 256, cmd) != NULL)
	{
		lines[count] = line;
		len = strlen(line);
		if (len>maxlen)
			maxlen = len;
		line = malloc(256*sizeof(char));
		count++;
	}
	pclose(cmd);

	/* position new window based on size */
	WINDOW *view_win = newwin(count, cols, rows-count, 0);

	/* print task data */
	wattrset(view_win, COLOR_PAIR(1));
	mvwhline(view_win, 0, 0, ' ', cols);
	tmp = (char *)eval_string(2*cols, cfg.formats.view, this, NULL, 0);
	umvaddstr_align(view_win, 0, tmp);
	free(tmp);
	wattrset(view_win, COLOR_PAIR(0));
	for (i=1; i<count-2; i++)
		mvwaddnstr(view_win, i, 1, lines[i], strlen(lines[i])-1);
	touchwin(view_win);
	wrefresh(view_win);

	/* wait for keypress */
	wgetch(view_win);

	/* cleanup */
	delwin(view_win);
	free(lines);

	/* force redraw */
	redraw = 1;
}
