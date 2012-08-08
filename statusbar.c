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
#include <string.h>
#include "common.h"
#include "log.h"
#include "statusbar.h"
#include "tasknc.h"

/* prompt index */
typedef struct _prompt_index
{
	char *prompt;
	char **history;
	struct _prompt_index *next;
} prompt_index;

/* global variables */
time_t sb_timeout = 0;                  /* when statusbar should be cleared */
prompt_index *prompt_number = NULL;     /* prompt index mapping head */

/* local functions */
static void add_to_history(prompt_index *, const char *);
static prompt_index *get_prompt_index(const char *);
static char *get_history(const prompt_index *, const int);
static void remove_first_char(char *);
static int replace_entry(char *, const int, const char *);

void add_to_history(prompt_index *pindex, const char *entry) /* {{{ */
{
	/* add an entry to history */
	int i;

	/* rotate history */
	for (i = cfg.history_max-1; i >= 1; i--)
		pindex->history[i] = pindex->history[i-1];

	/* add new entry */
	pindex->history[0] = strdup(entry);
} /* }}} */

void free_prompts() /* {{{ */
{
	/* free prompt data */
	prompt_index *last, *cur = prompt_number;
	int p;

	while (cur != NULL)
	{
		last = cur;
		cur = cur->next;
		for (p = 0; p < cfg.history_max && last->history[p] != NULL; p++)
			free(last->history[p]);
		free(last->history);
		free(last->prompt);
		free(last);
	}
} /* }}} */

char *get_history(const prompt_index *pindex, const int count) /* {{{ */
{
	/* get an item out of history */

	/* check for bad index */
	if (count >= cfg.history_max || count < 0)
		return NULL;

	return pindex->history[count];
} /* }}} */

prompt_index *get_prompt_index(const char *prompt) /* {{{ */
{
	/* find the index of a specified prompt, creating it if necessary */
	prompt_index *cur = prompt_number, *last = NULL;

	/* iterate through prompt indices */
	while (cur != NULL)
	{
		if (str_eq(prompt, cur->prompt))
			return cur;
		last = cur;
		cur = cur->next;
	}

	/* create a new prompt_index */
	cur = calloc(1, sizeof(prompt_index));
	cur->prompt = strdup(prompt);
	cur->history = calloc(cfg.history_max, sizeof(char *));
	cur->next = NULL;

	/* position new prompt_index */
	if (last != NULL)
		last->next = cur;
	else
		prompt_number = cur;

	return cur;
} /* }}} */

void remove_first_char(char *str) /* {{{ */
{
	/* remove the first character from a string, shift the remainder */
	while (*str != 0)
	{
		*str = *(str+1);
		str++;
	}
} /* }}} */

int replace_entry(char *str, const int len, const char *tmp) /* {{{ */
{
	/* replace a string with another and return the new length */
	int ret, i;

	/* empty new string */
	if (tmp == NULL)
	{
		ret = 0;
		goto done;
	}

	/* rotate in new chars */
	for (i=0; tmp[i] != 0; i++)
		str[i] = tmp[i];
	ret = i;
	goto done;

done:
	for (i=ret; i<len; i++)
		str[i] = 0;
	return ret;
} /* }}} */

int statusbar_getstr(char *str, const char *msg) /* {{{ */
{
	/* get a string from the user */
	int position = 0, c, histindex = -1, str_len = 0;
	bool done = false;
	const int msglen = strlen(msg);
	const prompt_index *pindex = get_prompt_index(msg);
	char *tmp;

	/* set up curses */
	set_curses_mode(NCURSES_MODE_STD);
	curs_set(1);            /* set cursor visible */

	/* get keys and buffer them */
	while (!done)
	{
		wipe_statusbar();
		umvaddstr(statusbar, 0, 0, "%s%s", msg, str);
		wmove(statusbar, 0, msglen+position);
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
				position--;
				while (position>=0 && str[position] == ' ')
				{
					str[position] = 0;
					position--;
					str_len--;
				}
				while (position>=0 && str[position] != ' ')
				{
					str[position] = 0;
					position--;
					str_len--;
				}
				position = position>=0 ? position : 0;
				str_len = str_len>=0 ? str_len : 0;
				break;
			case KEY_BACKSPACE:
			case 127:
				if (position <= 0)
					break;
				position--;
				remove_first_char(str+position);
				str_len = str_len>0 ? str_len-1 : 0;
				break;
			case KEY_DC:
				remove_first_char(str+position);
				str_len = str_len>0 ? str_len-1 : 0;
				break;
			case KEY_LEFT:
				position = position>0 ? position-1 : 0;
				break;
			case KEY_RIGHT:
				position = str[position] != 0 ? position+1 : position;
				break;
			case KEY_UP:
				histindex++;
				tmp = get_history(pindex, histindex);
				if (tmp == NULL)
					histindex = -1;
				str_len = replace_entry(str, str_len, tmp);
				break;
			case KEY_DOWN:
				histindex = histindex > 0 ? histindex-1 : 0;
				tmp = get_history(pindex, histindex);
				str_len = replace_entry(str, str_len, tmp);
				break;
			default:
				str[position] = c;
				position++;
				str_len++;
				break;
		}
	}

	/* reset curses */
	set_curses_mode(NCURSES_MODE_STD);

	/* add to history */
	add_to_history((prompt_index *)pindex, str);

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
