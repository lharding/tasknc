/*
 * tasklist.c - tasklist window
 * for tasknc
 * by mjheagle
 */

#define _XOPEN_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "color.h"
#include "common.h"
#include "config.h"
#include "formats.h"
#include "keys.h"
#include "log.h"
#include "sort.h"
#include "statusbar.h"
#include "tasklist.h"
#include "tasknc.h"
#include "tasks.h"
#include "pager.h"

/* local functions */
void tasklist_command_message(const int, const char*, const char*);

void key_tasklist_add() { /* {{{ */
    /* handle a keyboard direction to add new task */
    tasklist_task_add();
    reload = 1;
} /* }}} */

void key_tasklist_complete() { /* {{{ */
    /* complete selected task */
    task* cur = get_task_by_position(selline);
    int ret;

    statusbar_message(cfg.statusbar_timeout, "completing task");

    ret = task_background_command("task %s done");
    tasklist_remove_task(cur);

    tasklist_command_message(ret, "complete failed (%d)", "complete successful");
} /* }}} */

void key_tasklist_delete() { /* {{{ */
    /* complete selected task */
    task* cur = get_task_by_position(selline);
    int ret;

    statusbar_message(cfg.statusbar_timeout, "deleting task");

    ret = task_background_command("task %s delete");
    tasklist_remove_task(cur);

    tasklist_command_message(ret, "delete failed (%d)", "delete successful");
} /* }}} */

void key_tasklist_edit() { /* {{{ */
    /* edit selected task */
    task* cur = get_task_by_position(selline);
    int ret;
    char* uuid;

    statusbar_message(cfg.statusbar_timeout, "editing task");

    ret = task_interactive_command("task %s edit");
    uuid = strdup(cur->uuid);
    reload_task(cur);

    if (cfg.follow_task) {
        set_position_by_uuid(uuid);
        tasklist_check_curs_pos();
    }

    check_free(uuid);

    tasklist_command_message(ret, "edit failed (%d)", "edit succesful");
} /* }}} */

void key_tasklist_filter(const char* arg) { /* {{{ */
    /* handle a keyboard direction to add a new filter
     * arg - string to filter by (pass NULL to prompt user)
     *       see the manual page for how filter strings are parsed
     */
    check_free(active_filter);

    if (arg == NULL) {
        statusbar_getstr(&active_filter, "filter by: ");
        wipe_statusbar();
    } else
        active_filter = strdup(arg);

    /* force reload of task list */
    statusbar_message(cfg.statusbar_timeout, "filter applied");
    reload = true;
} /* }}} */

void key_tasklist_modify(const char* arg) { /* {{{ */
    /* handle a keyboard direction to modify a task
     * arg - the modifications to apply (pass NULL to prompt user)
     *       this will be appended to `task UUID modify `
     */
    char* argstr;

    if (arg == NULL) {
        statusbar_getstr(&argstr, "modify: ");
        wipe_statusbar();
    } else
        argstr = strdup(arg);

    task_modify(argstr);
    free(argstr);

    statusbar_message(cfg.statusbar_timeout, "task modified");
    redraw = true;
} /* }}} */

void key_tasklist_reload() { /* {{{ */
    /* wrapper function to handle keyboard instruction to reload task list */
    reload = true;
    statusbar_message(cfg.statusbar_timeout, "task list reloaded");
} /* }}} */

void key_tasklist_scroll(const int direction) { /* {{{ */
    /* handle a keyboard direction to scroll
     * direction - the direction to scroll in
     *             u = up one
     *             d = down one
     *             h = to first element in list
     *             e = to last element in list
     */
    const char oldsel = selline;
    const char oldoffset = pageoffset;

    switch (direction) {
    case 'u':

        /* scroll one up */
        if (selline > 0) {
            selline--;

            if (selline < pageoffset)
                pageoffset--;
        } else
            statusbar_message(cfg.statusbar_timeout, "already at top");

        break;

    case 'd':

        /* scroll one down */
        if (selline < taskcount - 1) {
            selline++;

            if (selline >= pageoffset + rows - 2)
                pageoffset++;
        } else
            statusbar_message(cfg.statusbar_timeout, "already at bottom");

        break;

    case 'h':
        /* go to first entry */
        pageoffset = 0;
        selline = 0;
        break;

    case 'e':

        /* go to last entry */
        if (taskcount > rows - 2)
            pageoffset = taskcount - rows + 2;

        selline = taskcount - 1;
        break;

    default:
        statusbar_message(cfg.statusbar_timeout, "invalid scroll direction");
        break;
    }

    if (pageoffset != oldoffset)
        redraw = true;
    else {
        if (oldsel - selline == 1)
            tasklist_print_task(selline, NULL, 2);
        else if (selline - oldsel == 1)
            tasklist_print_task(oldsel, NULL, 2);
        else {
            tasklist_print_task(oldsel, NULL, 1);
            tasklist_print_task(selline, NULL, 1);
        }
    }

    print_header();
    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "selline:%d offset:%d tasks:%d", selline, pageoffset, taskcount);
} /* }}} */

void key_tasklist_scroll_down() { /* {{{ */
    key_tasklist_scroll('d');
} /* }}} */

void key_tasklist_scroll_end() { /* {{{ */
    key_tasklist_scroll('e');
} /* }}} */

void key_tasklist_scroll_home() { /* {{{ */
    key_tasklist_scroll('h');
} /* }}} */

void key_tasklist_scroll_up() { /* {{{ */
    key_tasklist_scroll('u');
} /* }}} */

void key_tasklist_search(const char* arg) { /* {{{ */
    /* handle a keyboard direction to search
     * arg - the string to search for (pass NULL to prompt user)
     */
    check_free(searchstring);

    if (arg == NULL) {
        /* store search string  */
        statusbar_getstr(&searchstring, "/");
        wipe_statusbar();
    } else
        searchstring = strdup(arg);

    /* go to first result */
    find_next_search_result(head, get_task_by_position(selline));
    tasklist_check_curs_pos();
    redraw = true;
} /* }}} */

void key_tasklist_search_next() { /* {{{ */
    /* handle a keyboard direction to move to next search result */
    if (searchstring != NULL) {
        find_next_search_result(head, get_task_by_position(selline));
        tasklist_check_curs_pos();
        redraw = true;
    } else
        statusbar_message(cfg.statusbar_timeout, "no active search string");
} /* }}} */

void key_tasklist_sort(const char* arg) { /* {{{ */
    /* handle a keyboard direction to sort
     * arg - the mode to sort by (pass NULL to prompt user)
     *       see the manual page for how sort strings are parsed
     */
    task* cur;
    char* uuid = NULL;

    /* store selected task */
    cur = get_task_by_position(selline);

    if (cur != NULL)
        uuid = strdup(cur->uuid);

    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "sort: initial task uuid=%s", uuid);

    check_free(cfg.sortmode);

    if (arg == NULL) {
        /* store sort string  */
        cfg.sortmode = calloc(cols, sizeof(char));
        statusbar_getstr(&(cfg.sortmode), "sort by: ");
        sb_timeout = time(NULL) + 3;
    } else
        cfg.sortmode = strdup(arg);

    /* run sort */
    sort_wrapper(head);

    /* follow original task */
    if (cfg.follow_task) {
        set_position_by_uuid(uuid);
        tasklist_check_curs_pos();
    }

    check_free(uuid);

    /* force redraw */
    redraw = true;
} /* }}} */

void key_tasklist_sync() { /* {{{ */
    /* handle a keyboard direction to sync */
    int ret;

    statusbar_message(cfg.statusbar_timeout, "synchronizing tasks...");

    ret = task_interactive_command("yes n | task sync");

    if (ret == 0) {
        statusbar_message(cfg.statusbar_timeout, "tasks synchronized");
        reload = true;
    } else
        statusbar_message(cfg.statusbar_timeout, "task syncronization failed");
} /* }}} */

void key_tasklist_toggle_started() { /* {{{ */
    /* toggle whether a task is started */
    bool started;
    time_t now;
    task* cur = get_task_by_position(selline);
    char* cmdstr, *action, *actionpast, *reply;
    FILE* cmdout;
    int ret;

    /* check whether task is started */
    started = cur->start > 0;

    /* generate command */
    cmdstr = calloc(UUIDLENGTH + 16, sizeof(char));
    strcpy(cmdstr, "task ");
    strcat(cmdstr, cur->uuid);
    action = started ? " stop" : " start";
    strcat(cmdstr, action);

    /* run command */
    cmdout = popen(cmdstr, "r");
    free(cmdstr);
    ret = pclose(cmdout);

    /* check return value */
    if (WEXITSTATUS(ret) == 0) {
        time(&now);
        cur->start = started ? 0 : now;
        actionpast = started ? "stopped" : "started";
        asprintf(&reply, "task %s", actionpast);
        /* reset cached colors */
        cur->pair = -1;
        cur->selpair = -1;
    } else
        asprintf(&reply, "task%s failed (%d)", action, WEXITSTATUS(ret));

    statusbar_message(cfg.statusbar_timeout, reply);
    free(reply);
} /* }}} */

void key_tasklist_undo() { /* {{{ */
    /* handle a keyboard direction to run an undo */
    int ret;

    ret = task_background_command("task undo");

    if (ret == 0) {
        statusbar_message(cfg.statusbar_timeout, "undo executed");
        reload = true;
    } else
        statusbar_message(cfg.statusbar_timeout, "undo execution failed (%d)", ret);

    tasklist_check_curs_pos();
} /* }}} */

void key_tasklist_view() { /* {{{ */
    /* run task info on a task and display in pager */
    view_task(get_task_by_position(selline));
} /* }}} */

void tasklist_check_curs_pos() { /* {{{ */
    /* check if the cursor is in a valid position */
    const int onscreentasks = getmaxy(tasklist);

    /* log starting cursor position */
    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "cursor_check(init) - selline:%d offset:%d taskcount:%d perscreen:%d", selline, pageoffset, taskcount, rows - 3);

    /* check 0<=selline<taskcount */
    if (selline < 0)
        selline = 0;
    else if (selline >= taskcount)
        selline = taskcount - 1;

    /* check if page offset is necessary */
    if (taskcount <= onscreentasks)
        pageoffset = 0;
    /* offset up if necessary */
    else if (selline < pageoffset)
        pageoffset = selline;
    /* offset down if necessary */
    else if (taskcount > onscreentasks && pageoffset + onscreentasks - 1 < selline)
        pageoffset = selline - onscreentasks + 1;
    /* dont allow blank lines if there is an offset */
    else if (taskcount > onscreentasks && taskcount - pageoffset < onscreentasks)
        pageoffset = taskcount - onscreentasks;

    /* log cursor position */
    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "cursor_check - selline:%d offset:%d taskcount:%d perscreen:%d", selline, pageoffset, taskcount, rows - 3);
} /* }}} */

void tasklist_command_message(const int ret, const char* fail, const char* success) { /* {{{ */
    /* print a message depending on the return of a command
     * ret     - the return of the command
     * fail    - the format string to use if ret == 1
     * success - the literal string to use if ret == 0
     */
    if (ret != 0)
        statusbar_message(cfg.statusbar_timeout, fail, ret);
    else
        statusbar_message(cfg.statusbar_timeout, success);
} /* }}} */

void tasklist_window() { /* {{{ */
    /* ncurses main function */
    int c;
    task* cur;
    char* uuid = NULL;

    /* get field lengths */
    cfg.fieldlengths.project = max_project_length();
    cfg.fieldlengths.date = DATELENGTH;

    /* create windows */
    rows = LINES;
    cols = COLS;
    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "rows: %d, columns: %d", rows, cols);
    header = newwin(1, cols, 0, 0);
    tasklist = newwin(rows - 2, cols, 1, 0);
    statusbar = newwin(1, cols, rows - 1, 0);
    tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "ncurses windows: h:%p, t:%p, s:%p (%d,%d)", header, tasklist, statusbar, rows, cols);

    if (statusbar == NULL || tasklist == NULL || header == NULL) {
        tnc_fprintf(logfp, LOG_ERROR, "window creation failed (rows:%d, cols:%d)", rows, cols);
        ncurses_end(-1);
    }

    /* set curses settings */
    set_curses_mode(NCURSES_MODE_STD);

    /* print task list */
    check_screen_size();
    cfg.fieldlengths.description = COLS - cfg.fieldlengths.project - 1 - cfg.fieldlengths.date;
    task_count();
    print_header();
    tasklist_print_task_list();

    /* main loop */
    while (1) {
        /* set variables for determining actions */
        done = false;
        redraw = false;
        reload = false;

        /* check for an empty task list */
        if (head == NULL) {
            if (strcmp(active_filter, "") == 0) {
                tnc_fprintf(logfp, LOG_ERROR, "it appears that your task list is empty. %s does not yet support empty task lists.", PROGNAME);
                ncurses_end(-1);
            }

            active_filter = strdup("");
            reload = true;
        }

        /* get the screen size */
        rows = LINES;
        cols = COLS;

        /* check for a screen thats too small */
        check_screen_size();

        /* check for size changes */
        check_resize();

        /* apply staged window updates */
        doupdate();

        /* get a character */
        c = wgetch(statusbar);

        /* handle the character */
        handle_keypress(c, MODE_TASKLIST);

        /* exit */
        if (done)
            break;

        /* reload task list */
        if (reload) {
            cur = get_task_by_position(selline);

            if (cur != NULL)
                uuid = strdup(cur->uuid);

            wipe_tasklist();
            reload_tasks();
            task_count();
            redraw = true;

            if (cfg.follow_task)
                set_position_by_uuid(uuid);

            check_free(uuid);
            uuid = NULL;
            tasklist_check_curs_pos();
        }

        /* redraw all windows */
        if (redraw) {
            cfg.fieldlengths.project = max_project_length();
            cfg.fieldlengths.description = cols - cfg.fieldlengths.project - 1 - cfg.fieldlengths.date;
            print_header();
            tasklist_print_task_list();
            tasklist_check_curs_pos();
            touchwin(tasklist);
            touchwin(header);
            touchwin(statusbar);
            wnoutrefresh(tasklist);
            wnoutrefresh(header);
            wnoutrefresh(statusbar);
            doupdate();
        }

        statusbar_timeout();
    }
} /* }}} */

void tasklist_print_task(const int tasknum, const task* this, const int count) { /* {{{ */
    /* print a task specified by number
     * tasknum - the number of the task to be printed (used to find task object
     *           if it is not specified in `this`
     * this    - a pointer to the task to be printed
     *           only one of either `tasknum` or `this` should be specified
     * count   - number of consecutive tasks to print
     */
    bool sel = false;
    char* tmp;
    int x, y;

    /* determine position to print */
    y = tasknum - pageoffset;

    if (y < 0 || y >= rows - 1)
        return;

    /* find task pointer if necessary */
    if (this == NULL)
        this = get_task_by_position(tasknum);

    /* check if this is NULL */
    if (this == NULL) {
        tnc_fprintf(logfp, LOG_ERROR, "task %d is null", tasknum);
        return;
    }

    /* determine if line is selected */
    if (tasknum == selline)
        sel = true;

    /* wipe line */
    wattrset(tasklist, COLOR_PAIR(0));

    for (x = 0; x < cols; x++)
        mvwaddch(tasklist, y, x, ' ');

    /* evaluate line */
    wmove(tasklist, 0, 0);
    wattrset(tasklist, get_colors(OBJECT_TASK, (task*)this, sel));
    tmp = (char*)eval_format(cfg.formats.task_compiled, (task*)this);
    umvaddstr_align(tasklist, y, tmp);
    free(tmp);

    /* print next task if requested */
    if (count > 1)
        tasklist_print_task(tasknum + 1, this->next, count - 1);
    else
        wnoutrefresh(tasklist);
} /* }}} */

void tasklist_print_task_list() { /* {{{ */
    /* print every task in the task list */
    task* cur;
    short counter = 0;

    cur = head;

    while (cur != NULL) {
        tasklist_print_task(counter, cur, 1);

        /* move to next item */
        counter++;
        cur = cur->next;
    }

    if (counter < cols - 2)
        wipe_screen(tasklist, counter - pageoffset, rows - 2);
} /* }}} */

void tasklist_remove_task(task* this) { /* {{{ */
    /* remove a task from the task list without reloading */
    if (this == head)
        head = this->next;
    else
        this->prev->next = this->next;

    if (this->next != NULL)
        this->next->prev = this->prev;

    free_task(this);
    taskcount--;
    tasklist_check_curs_pos();
    redraw = true;
} /* }}} */

void tasklist_task_add() { /* {{{ */
    /* create a new task by adding a generic task
     * then letting the user edit it
     */
    FILE* cmdout;
    char* cmd, line[TOTALLENGTH], *failmsg;
    unsigned short tasknum;
    int ret = 0, pret;

    /* add new task */
    cmd = strdup("task add new task");
    tnc_fprintf(logfp, LOG_DEBUG, "running: %s", cmd);
    cmdout = popen(cmd, "r");

    while (fgets(line, sizeof(line) - 1, cmdout) != NULL) {
        if (sscanf(line, "Created task %hu.", &tasknum))
            break;
    }

    pret = pclose(cmdout);
    free(cmd);

    if (WEXITSTATUS(pret) != 0) {
        failmsg = strdup("task add failed (%d)");
        goto done;
    }

    /* edit task */
    if (cfg.version[0] < '2')
        asprintf(&cmd, "task edit %d", tasknum);
    else
        asprintf(&cmd, "task %d edit", tasknum);

    ret = task_interactive_command(cmd);
    free(cmd);
    failmsg = strdup("task edit failed (%s)");
    goto done;

done:
    tasklist_command_message(ret, failmsg, "task add succeeded");
    free(failmsg);
} /* }}} */

// vim: noet ts=4 sw=4 sts=4
