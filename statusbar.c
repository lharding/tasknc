/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * statusbar.c - statusbar functions
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE

#include <curses.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include "common.h"
#include "log.h"
#include "statusbar.h"
#include "tasknc.h"

/* global variables */
time_t sb_timeout = 0;                  /* when statusbar should be cleared */

int statusbar_getstr(char *str, const char *msg) /* {{{ */
{
	/* get a string from the user */
	int position = 0, c;
	bool done = false;

	/* set up curses */
	set_curses_mode(NCURSES_MODE_STD);
	curs_set(1);            /* set cursor visible */

	/* get keys and buffer them */
	while (!done)
	{
		wipe_statusbar();
		umvaddstr(statusbar, 0, 0, "%s%s", msg, str);
		wrefresh(statusbar);

		c = wgetch(statusbar);
		switch (c)
		{
			case ERR:
				break;
			case '\r':
			case '\n':
				done = true;
				break;
			case 23: /* C-w */
				while (position>=0 && str[position] == ' ')
				{
					str[position] = 0;
					position--;
				}
				while (position>=0 && str[position] != ' ')
				{
					str[position] = 0;
					position--;
				}
				position = position>=0 ? position : 0;
				break;
			case KEY_BACKSPACE:
			case KEY_DC:
			case 127:
				str[position] = 0;
				position--;
				break;
			default:
				str[position] = c;
				position++;
				break;
		}
	}

	/* reset curses */
	set_curses_mode(NCURSES_MODE_STD);

	return position;
} /* }}} */

void statusbar_message(const int dtmout, const char *format, ...) /* {{{ */
{
	/* print a message in the statusbar */
	va_list args;
	char *message;

	/* format message */
	va_start(args, format);
	vasprintf(&message, format, args);
	va_end(args);

	/* check for active screen */
	if (stdscr == NULL)
	{
		if (cfg.loglvl >= LOG_DEBUG)
			tnc_fprintf(stdout, LOG_INFO, message);
		tnc_fprintf(logfp, LOG_DEBUG, "(stdscr==NULL) %s", message);
		free(message);
		return;
	}

	wipe_statusbar();

	/* print message */
	umvaddstr(statusbar, 0, 0, message);
	free(message);

	/* set timeout */
	if (dtmout>=0)
		sb_timeout = time(NULL) + dtmout;

	/* refresh now or at next doupdate depending on time */
	if (dtmout<0)
		wrefresh(statusbar);
	else
		wnoutrefresh(statusbar);
} /* }}} */

void statusbar_timeout() /* {{{ */
{
	/* timeout statusbar */
	if (sb_timeout>0 && sb_timeout<time(NULL))
	{
		sb_timeout = 0;
		wipe_statusbar();
	}
} /* }}} */
