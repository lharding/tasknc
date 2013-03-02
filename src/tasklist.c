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
int print_task(struct nc_win * nc, const int line, const int width, struct task * task, struct config * conf) {
        /* check for NULL task */
        if (task == NULL)
                return ERR;

        /* determine if task is selected */
        const bool is_selected = (line == nc->selline - nc->offset);

        /* generate string to display */
        char * str = task_snprintf(width, conf_get_task_format(conf), task);

        /* set color */
        /* TODO: determine color based on rules */
        if (is_selected)
                wattrset(nc->win, COLOR_PAIR(1));

        /* display string */
        int ret = umvaddstr(nc->win, line, 0, width, "%s", str);
        free(str);

        /* reset color */
        wattrset(nc->win, COLOR_PAIR(0));

        return ret;
}

int tasklist_window(struct task ** tasks, struct config * conf) {
        /* ncurses main function */
        initscr();
        int rows = LINES;
        int cols = COLS;

        /* create windows */
        struct nc_win * tasklist = make_window(rows-2, cols, 1, 0, TASKNC_TASKLIST);
        if (tasklist == NULL) {
                endwin();
                return 1;
        }
        set_curses_mode(conf, tasklist->win, NCURSES_MODE_STD_BLOCKING);

        /* initialize colors */
        /* TODO: do this in configuration */
        if (start_color() == ERR)
                return -2;
        if (assume_default_colors(-1, -1) == ERR)
                return -3;
        if (init_pair(1, COLOR_BLUE, -1) == ERR)
                return -1;

        /* print test lines */
        int n;
        for (n = 0; n < rows-2 && tasks[n] != NULL; n++)
                print_task(tasklist, n, cols, tasks[n], conf);
        wrefresh(tasklist->win);
        wgetch(tasklist->win);

        /* close windows */
        delwin(tasklist->win);
        endwin();

        return 0;
}
