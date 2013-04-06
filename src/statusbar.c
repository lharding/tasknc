/*
 * statusbar window for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <curses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cursutil.h"
#include "configure.h"
#include "keybind.h"
#include "statusbar.h"

/* function to wipe the statusbar */
int statusbar_wipe(struct nc_win * bar) {
        int n;
        for (n = 0; n < COLS; n++)
                mvwaddch(bar->win, 0, n, ' ');
        wrefresh(bar->win);
        return OK;
}

/* keybind to wipe the statusbar */
int statusbar_clear(struct bindarg * arg) {
        return statusbar_wipe(arg->statusbar);
}

/* print a formatted string to the statusbar */
int statusbar_printf(struct bindarg * arg, const char *format, ...) {
        va_list args;
        char *message = calloc(COLS+1, sizeof(char));

        /* format message */
        va_start(args, format);
        vsnprintf(message, COLS, format, args);
        va_end(args);

        statusbar_wipe(arg->statusbar);

        /* print message */
        int ret = umvaddstr(arg->statusbar->win, 0, 0, COLS, "%s", message);
        free(message);

        wrefresh(arg->statusbar->win);
        return ret;
}

/* remove the first character from a string, shift the remainder back */
void remove_first_char(wchar_t *str) {
        while (*str != 0) {
                *str = *(str+1);
                str++;
        }
}

/* get string input */
int statusbar_get_string(struct bindarg * arg, const char *msg, char **str) {
        WINDOW *win = arg->statusbar->win;
        int position = 0, histindex = -1, str_len = 0, charlen, ret = 0;
        bool done = false;
        const int msglen = strlen(msg);
        /* const prompt_index *pindex = get_prompt_index(msg); */
        wchar_t *tmp, *wstr = calloc(3*COLS, sizeof(wchar_t));
        wint_t c;

        /* set cursor */
        curs_set(1);

        /* get keys and buffer them */
        while (!done) {
                statusbar_wipe(arg->statusbar);
                umvaddstr(win, 0, 0, COLS, "%s", msg);
                mvwaddnwstr(win, 0, msglen, wstr, str_len);
                wmove(win, 0, msglen+position);
                wrefresh(win);

                ret = wget_wch(win, &c);
                if (ret == ERR)
                        continue;

                switch (c) {
                        case ERR:
                                break;
                        case '\r':
                        case '\n':
                                done = true;
                                break;
                        case 21: /* C-u (discard line) */
                                position--;
                                while (position>=0) {
                                        wstr[position] = 0;
                                        position--;
                                        str_len--;
                                }
                                position = 0;
                                str_len = 0;
                                break;
                        case 23: /* C-w (delete last word) */
                                position--;
                                while (position>=0 && wstr[position] == ' ') {
                                        wstr[position] = 0;
                                        position--;
                                        str_len--;
                                }
                                while (position>=0 && wstr[position] != ' ') {
                                        wstr[position] = 0;
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
                                remove_first_char(wstr+position);
                                str_len = str_len>0 ? str_len-1 : 0;
                                break;
                        case KEY_DC:
                                remove_first_char(wstr+position);
                                str_len = str_len>0 ? str_len-1 : 0;
                                break;
                        case KEY_LEFT:
                                position = position>0 ? position-1 : 0;
                                break;
                        case KEY_RIGHT:
                                position = wstr[position] != 0 ? position+1 : position;
                                break;
                        case KEY_UP:
                                /* histindex++; */
                                /* position = 0; */
                                /* tmp = get_history(pindex, histindex); */
                                /* if (tmp == NULL) */
                                        /* histindex = -1; */
                                /* str_len = replace_entry(wstr, str_len, tmp); */
                                break;
                        case KEY_DOWN:
                                /* histindex = histindex > 0 ? histindex-1 : 0; */
                                /* position = 0; */
                                /* tmp = get_history(pindex, histindex); */
                                /* str_len = replace_entry(wstr, str_len, tmp); */
                                break;
                        case KEY_HOME:
                                position = 0;
                                break;
                        case KEY_END:
                                position = str_len;
                                break;
                        default:
                                wstr[position] = c;
                                position++;
                                str_len++;
                                break;
                }
        }

        /* convert wchar_t to char */
        charlen = wcstombs(NULL, wstr, 0)+1;
        *str = calloc(charlen, sizeof(char));
        wcstombs(*str, wstr, charlen);

        /* add to history */
        /* add_to_history((prompt_index *)pindex, wstr); */

        /* reset cursor */
        curs_set(0);

        return ret;
}
