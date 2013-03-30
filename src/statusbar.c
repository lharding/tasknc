/*
 * statusbar window for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <curses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
