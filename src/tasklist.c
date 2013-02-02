/*
 * tasklist window for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "tasklist.h"

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
        WINDOW *tasklist = newwin(rows-2, cols, 1, 0);

        /* print test lines */
        int n;
        for (n = 0; n < rows-2 && tasks[n] != NULL; n++)
                simple_print_task(tasklist, n, cols, tasks[n]);
        wrefresh(tasklist);
        wgetch(tasklist);

        /* close windows */
        delwin(tasklist);
        endwin();
}
