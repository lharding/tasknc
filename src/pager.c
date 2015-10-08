/*
 * view.c - view task info
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "color.h"
#include "common.h"
#include "config.h"
#include "formats.h"
#include "keys.h"
#include "log.h"
#include "pager.h"
#include "statusbar.h"
#include "tasklist.h"
#include "tasknc.h"

/* local functions */
static void pager_window(line* head,
                         const bool fullscreen,
                         int nlines,
                         char* title);

/* global variables */
int offset, height, linecount;
bool pager_done;

void free_lines(line* head) { /* {{{ */
    /* iterate through linked list of lines and free all elements */
    line* cur, *last;

    cur = head;

    while (cur != NULL) {
        last = cur;
        cur = cur->next;
        free(last->str);
        free(last);
    }
} /* }}} */

void help_window() { /* {{{ */
    /* display a help window */
    line* head, *cur, *last;
    keybind* this;
    char* modestr, *keyname;
    static bool help_running = false;

    /* check for existing help window */
    if (help_running) {
        statusbar_message(cfg.statusbar_timeout, "help window already open");
        return;
    }

    /* lock help window */
    help_running = true;

    /* list keybinds */
    head = calloc(1, sizeof(line));
    head->str = strdup("keybinds");

    last = head;
    this = keybinds;

    while (this != NULL) {
        if (this->key == ERR || this->key == KEY_RESIZE) {
            this = this->next;
            continue;
        }

        cur = calloc(1, sizeof(line));
        last->next = cur;

        if (this->mode == MODE_TASKLIST) {
            modestr = "tasklist";
        } else if (this->mode == MODE_PAGER) {
            modestr = "pager   ";
        } else {
            modestr = "unknown ";
        }

        keyname = name_key(this->key);

        if (this->argstr == NULL) {
            asprintf(&(cur->str), "%8s    %-8s    %s", keyname, modestr,
                     name_function(this->function));
        } else {
            asprintf(&(cur->str), "%8s    %-8s    %s %s", keyname, modestr,
                     name_function(this->function), this->argstr);
        }

        free(keyname);
        this = this->next;
        last = cur;
    }

    pager_window(head, 1, -1, " help");
    free_lines(head);
    help_running = false;
} /* }}} */

void key_pager_close() { /* {{{ */
    /* close the pager on keypress */
    pager_done = true;
} /* }}} */

void key_pager_scroll_down() { /* {{{ */
    /* scroll down a line in pager */
    if (offset < linecount + 1 - height) {
        offset++;
    } else {
        statusbar_message(cfg.statusbar_timeout, "already at bottom");
    }
} /* }}} */

void key_pager_scroll_end() { /* {{{ */
    /* scroll to end of pager */
    offset = linecount > height ? linecount - height + 1 : 0;
} /* }}} */

void key_pager_scroll_home() { /* {{{ */
    /* scroll to beginning of pager */
    offset = 0;
} /* }}} */

void key_pager_scroll_up() { /* {{{ */
    /* scroll up a line in pager */
    if (offset > 0) {
        offset--;
    } else {
        statusbar_message(cfg.statusbar_timeout, "already at top");
    }
} /* }}} */

void pager_command(const char* cmdstr, const char* title, const bool fullscreen,
                   const int head_skip, const int tail_skip) { /* {{{ */
    /**
     * run a command and page through its results
     * cmdstr     - the command to be run
     * title      - the title of the pager
     * fullscreen - whether the pager should be fullscreen
     * head_skip  - how many lines to skip at the beginning of output
     * tail_skip  - how many lines to skip at the end of output
     */
    FILE* cmd;
    char* str;
    int count = 0, maxlen = 0, len;
    line* head = NULL, *last = NULL, *cur;

    /* run command, gathering strs into a buffer */
    cmd = popen(cmdstr, "r");
    str = calloc(TOTALLENGTH, sizeof(char));

    while (fgets(str, TOTALLENGTH, cmd) != NULL) {
        /* determine max width */
        len = strlen(str);

        if (len > maxlen) {
            maxlen = len;
        }

        /* create line */
        cur = calloc(1, sizeof(line));
        cur->str = str;

        /* place line in list */
        if (count == head_skip) {
            head = cur;
        } else if (count < head_skip) {
            free(cur->str);
            free(cur);
        } else if (last != NULL) {
            last->next = cur;
        }

        /* move to next line */
        str = calloc(256, sizeof(char));
        count++;
        last = cur;
    }

    free(str);
    pclose(cmd);
    count -= tail_skip;

    /* run pager */
    pager_window(head, fullscreen, count, (char*)title);

    /* free lines */
    free_lines(head);
} /* }}} */

void pager_window(line* head, const bool fullscreen, int nlines,
                  char* title) { /* {{{ */
    /**
     * page through a linked list of lines
     * head       - the first line to print
     * fullscreen - whether the pager should be fullscreen
     * nlines     - the number of lines that will be printed
     * title      - the title of the pager
     */
    int startx, starty, lineno, c, taskheight;
    line* tmp;
    offset = 0;
    WINDOW* last_pager = NULL;
    const int orig_offset = offset;
    const int orig_height = height;
    const int orig_linecount = linecount;

    /* store previous pager window if necessary */
    if (pager != NULL) {
        last_pager = pager;
    }

    /* count lines if necessary */
    if (nlines <= 0) {
        tmp = head;
        nlines = 0;

        while (tmp != NULL) {
            nlines++;
            tmp = tmp->next;
        }
    }

    /* exit if there are no lines */
    tnc_fprintf(logfp, LOG_DEBUG, "pager: linecount=%d", linecount);

    if (nlines == 0) {
        return;
    }

    linecount = nlines;

    /* determine screen dimensions and create window */
    taskheight = getmaxy(tasklist);

    if (fullscreen) {
        height = taskheight;
        starty = 1;
        startx = 0;
    } else {
        height = linecount + 1 < taskheight ? linecount + 1 : taskheight;
        starty = rows - height - 1;
        startx = 0;
    }

    tnc_fprintf(logfp, LOG_DEBUG, "pager: h=%d w=%d y=%d x=%d", height, cols,
                starty, startx);
    pager = newwin(height, cols, starty, startx);

    /* check if pager was created */
    if (pager == NULL) {
        statusbar_message(cfg.statusbar_timeout, "failed to create pager window");
        tnc_fprintf(logfp, LOG_ERROR, "failed to create pager window");
        return;
    }

    pager_done = false;

    while (1) {
        tnc_fprintf(logfp, LOG_DEBUG, "offset:%d height:%d lines:%d", offset, height,
                    linecount);

        /* print title */
        wattrset(pager, get_colors(OBJECT_HEADER, NULL, NULL));
        mvwhline(pager, offset, 0, ' ', cols);
        umvaddstr_align(pager, offset, title);

        /* print lines */
        wattrset(pager, COLOR_PAIR(0));
        tmp = head;
        lineno = 1;

        while (tmp != NULL && lineno <= linecount && lineno - offset <= height) {
            if (lineno > offset) {
                mvwhline(pager, lineno - offset, 0, ' ', cols);
                umvaddstr(pager, lineno - offset, 0, tmp->str);
            }

            lineno++;
            tmp = tmp->next;
        }

        touchwin(pager);
        wrefresh(pager);

        /* accept keys */
        c = wgetch(statusbar);
        handle_keypress(c, MODE_PAGER);

        if (pager_done) {
            pager_done = false;
            break;
        }

        statusbar_timeout();
    }

    /* destroy window and force redraw of tasklist */
    delwin(pager);

    if (last_pager == NULL) {
        pager = NULL;
        redraw = true;
    } else {
        tasklist_print_task_list();
        pager = last_pager;
    }

    /* reset original values */
    offset    = orig_offset;
    height    = orig_height;
    linecount = orig_linecount;
} /* }}} */

void view_stats() { /* {{{ */
    /* run `task stat` and page the output */
    char* cmdstr;
    asprintf(&cmdstr, "task stat rc._forcecolor=no rc.defaultwidth=%d 2>&1",
             cols - 4);
    const char* title = "task statistics";
    static bool stats_running = false;

    /* check for an existing stats window */
    if (stats_running) {
        statusbar_message(cfg.statusbar_timeout, "stats window already open");
        return;
    }

    /* lock stats window */
    stats_running = true;

    /* run pager */
    pager_command(cmdstr, title, 1, 1, 4);

    /* clean up */
    stats_running = false;
    free(cmdstr);
} /* }}} */

void view_task(task* this) { /* {{{ */
    /* run `task info` and print it to a window */
    char* cmdstr, *title;

    /* build command and title */
    asprintf(&cmdstr, "task %s info rc._forcecolor=no rc.defaultwidth=%d 2>&1",
             this->uuid, cols - 4);
    title = (char*)eval_format(cfg.formats.view_compiled, this);

    /* run pager */
    pager_command(cmdstr, title, 0, 1, 4);

    /* clean up */
    free(cmdstr);
    free(title);
} /* }}} */

// vim: et ts=4 sw=4 sts=4
