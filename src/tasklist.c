/*
 * tasklist window for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "tasklist.h"
#include "cursutil.h"

/* temporary function to test printing tasks */
int simple_print_task(WINDOW *win, const int line, const int width, struct task * task) {
        if (task == NULL)
                return ERR;
        return umvaddstr(win, line, 0, width, "%3d %10s %s", task_get_index(task), task_get_project(task), task_get_description(task));
}

void tasklist_window(struct task ** tasks) {
        /* ncurses main function */
        initscr();
        int rows = LINES;
        int cols = COLS;

        /* create windows */
        struct nc_win * tasklist = make_window(rows-2, cols, 1, 0, TASKNC_TASKLIST);
        if (tasklist == NULL) {
                endwin();
                return;
        }
        set_curses_mode(tasklist->win, NCURSES_MODE_STD_BLOCKING);

        /* print test lines */
        int n;
        for (n = 0; n < rows-2 && tasks[n] != NULL; n++)
                simple_print_task(tasklist->win, n, cols, tasks[n]);
        wrefresh(tasklist->win);
        wgetch(tasklist->win);

        /* close windows */
        delwin(tasklist->win);
        endwin();
}
