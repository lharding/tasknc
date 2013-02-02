/*
 * ncurses utilities for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cursutil.h"

/* evaluate a format string, convert to wchar, and print it to window */
int umvaddstr(WINDOW *win, const int y, const int x, const int width, const char *format, ...)
{
        int len, r;
        wchar_t *wstr;
        char *str;
        va_list args;

        /* build str */
        va_start(args, format);
        const int slen = sizeof(wchar_t)*(width-x+1)/sizeof(char);
        str = calloc(slen, sizeof(char));
        vsnprintf(str, slen-1, format, args);
        va_end(args);

        /* allocate wchar_t string */
        len = strlen(str)+1;
        wstr = calloc(len, sizeof(wchar_t));

        /* check for valid allocation */
        if (wstr==NULL)
                return -1;

        /* perform conversion and write to screen */
        mbstowcs(wstr, str, len);
        len = wcslen(wstr);
        if (len>width-x)
                len = width-x;
        r = mvwaddnwstr(win, y, x, wstr, len);

        /* free memory allocated */
        free(wstr);
        free(str);

        return r;
}

/* function to split string and print aligned sections */
int umvaddstr_align(WINDOW *win, const int y, const int width, char *str)
{
        char *right, *pos;
        int ret, tmp;

        /* print background line */
        mvwhline(win, y, 0, ' ', width);

        /* find break */
        pos = strstr(str, "$>");

        /* end left string */
        if (pos != NULL)
        {
                (*pos) = 0;
                right = (pos+2);
        }
        else
                right = NULL;

        /* print strings */
        tmp = umvaddstr(win, y, 0, width, str);
        ret = tmp;
        if (right != NULL)
                ret = umvaddstr(win, y, width-strlen(right), width-strlen(right), right);
        if (tmp > ret)
                ret = tmp;

        /* replace the '$' in the string */
        if (pos != NULL)
                (*pos) = '$';

        return ret;
}

/* function to change curses input modes */
void set_curses_mode(WINDOW *win, const enum ncurses_mode mode)
{
        switch (mode)
        {
                case NCURSES_MODE_STD:
                        keypad(win, TRUE);      /* enable keyboard mapping */
                        nonl();                 /* tell curses not to do NL->CR/NL on output */
                        cbreak();               /* take input chars one at a time, no wait for \n */
                        noecho();               /* dont echo input */
                        curs_set(0);            /* set cursor invisible */
                        wtimeout(win, 1000);    /* timeout getch */
                        break;
                case NCURSES_MODE_STD_BLOCKING:
                        keypad(win, TRUE);      /* enable keyboard mapping */
                        nonl();                 /* tell curses not to do NL->CR/NL on output */
                        cbreak();               /* take input chars one at a time, no wait for \n */
                        noecho();               /* dont echo input */
                        curs_set(0);            /* set cursor invisible */
                        wtimeout(win, -1);      /* no timeout on getch */
                        break;
                case NCURSES_MODE_STRING:
                        curs_set(1);            /* set cursor visible */
                        nocbreak();             /* wait for \n */
                        echo();                 /* echo input */
                        wtimeout(win, -1);      /* no timeout on getch */
                        break;
                default:
                        break;
        }
}
