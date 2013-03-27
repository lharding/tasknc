/*
 * tasklist window for tasknc
 * by mjheagle
 */

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "cursutil.h"
#include "keybind.h"
#include "sort.h"
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
int tasklist_scroll_down(struct bindarg *arg) {
        struct nc_win *win = arg->win;
        struct tasklist *list = arg->list;

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

int tasklist_scroll_up(struct bindarg *arg) {
        struct nc_win *win = arg->win;

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

int tasklist_scroll_home(struct bindarg *arg) {
        struct nc_win *win = arg->win;

        win->selline = 0;
        win->offset = 0;

        return 1;
}

int tasklist_scroll_end(struct bindarg *arg) {
        struct nc_win *win = arg->win;
        struct tasklist *list = arg->list;

        win->selline = list->ntasks-1;
        if (list->ntasks > win->height)
                win->offset = list->ntasks - win->height;

        return 1;
}

/* task actions */
int tasklist_reload(struct bindarg *arg) {
        int ret = reload_tasklist(arg->list, conf_get_filter(arg->conf));
        if (ret == 0)
                sort_tasks(arg->list->tasks, 0, conf_get_sort(arg->conf));

        return ret;
}

int tasklist_complete_tasks(struct bindarg *arg) {
        int ret;
        int *indexes = calloc(2, sizeof(int));
        indexes[0] = arg->win->selline;

        ret = task_complete(arg->list, indexes, 1, conf_get_filter(arg->conf));
        free(indexes);

        if (ret == 0)
                sort_tasks(arg->list->tasks, 0, conf_get_sort(arg->conf));

        return ret;
}

int tasklist_delete_tasks(struct bindarg *arg) {
        int ret;
        int *indexes = calloc(2, sizeof(int));
        indexes[0] = arg->win->selline;

        ret = task_delete(arg->list, indexes, 1, conf_get_filter(arg->conf));
        free(indexes);

        if (ret == 0)
                sort_tasks(arg->list->tasks, 0, conf_get_sort(arg->conf));

        return ret;
}

int tasklist_undo(struct bindarg *arg) {
        int ret = task_undo(arg->list, conf_get_filter(arg->conf));
        if (ret == 0)
                sort_tasks(arg->list->tasks, 0, conf_get_sort(arg->conf));

        return ret;
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
        add_keybind(binds, 'g', tasklist_scroll_home);
        add_keybind(binds, 'G', tasklist_scroll_end);
        add_keybind(binds, 'r', tasklist_reload);
        add_keybind(binds, 'c', tasklist_complete_tasks);
        add_keybind(binds, 'd', tasklist_delete_tasks);
        add_keybind(binds, 'u', tasklist_undo);

        /* create bindarg structure */
        struct bindarg arg;
        arg.win = tasklist;
        arg.list = list;
        arg.conf = conf;

        /* print test lines */
        while (true) {
                int n;
                for (n = tasklist->offset; n < tasklist->offset + rows - 2 && list->tasks[n] != NULL; n++)
                        print_task(tasklist, n - tasklist->offset, cols, list->tasks[n], conf);
                wrefresh(tasklist->win);
                int key = wgetch(tasklist->win);
                if (key == 'q')
                        break;
                int ret = eval_keybind(binds, key, &arg);
        }

        /* free keybinds */
        free_keybind_list(binds);

        /* close windows */
        delwin(tasklist->win);
        endwin();

        return 0;
}
