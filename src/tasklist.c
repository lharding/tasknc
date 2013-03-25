/*
 * tasklist window for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "cursutil.h"
#include "keybind.h"
#include "tasklist.h"

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

        /* wipe line */
        int n;
        for (n = 0; n < width; n++)
                mvwaddch(nc->win, line, n, ' ');

        /* display string */
        int ret = umvaddstr(nc->win, line, 0, width, "%s", str);
        free(str);

        /* reset color */
        wattrset(nc->win, COLOR_PAIR(0));

        return ret;
}

/* scrolling functions */
int tasklist_scroll_down(struct config *conf, struct tasklist *list, struct nc_win *win) {
        /* check if scroll is possible */
        if (win->selline >= list->ntasks - 1)
                return 0;

        /* increment selected line */
        win->selline++;

        /* check if offset needs to be increased */
        if (win->selline >= win->offset+win->height)
                win->offset++;

        return 1;
}

int tasklist_scroll_up(struct config *conf, struct tasklist *list, struct nc_win *win) {
        /* check if scroll is possible */
        if (win->selline < 1)
                return 0;

        /* increment selected line */
        win->selline--;

        /* check if offset needs to be decreased */
        if (win->selline < win->offset)
                win->offset = win->selline;

        return 1;
}

/* display an array of tasks in a ncurses window */
int tasklist_window(struct tasklist * list, struct config * conf) {
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

        /* apply default keybinds */
        struct keybind_list *binds;
        binds = new_keybind_list();
        add_keybind(binds, 'j', tasklist_scroll_down);
        add_keybind(binds, 'k', tasklist_scroll_up);

        /* print test lines */
        while (true) {
                int n;
                for (n = tasklist->offset; n < tasklist->offset + rows - 2 && list->tasks[n] != NULL; n++)
                        print_task(tasklist, n - tasklist->offset, cols, list->tasks[n], conf);
                wrefresh(tasklist->win);
                int key = wgetch(tasklist->win);
                if (key == 'q')
                        break;
                int ret = eval_keybind(binds, key, conf, list, tasklist);
        }

        /* free keybinds */
        free_keybind_list(binds);

        /* close windows */
        delwin(tasklist->win);
        endwin();

        return 0;
}
