/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * tasklist.c - tasklist window
 * for tasknc
 * by mjheagle
 */

#ifndef _TASKLIST_H
#define _TASKLIST_H

#include <time.h>
#include "common.h"

void key_tasklist_add();
void key_tasklist_filter(const char *arg);
void key_tasklist_modify(const char *arg);
void key_tasklist_reload();
void key_tasklist_scroll(const int direction);
void key_tasklist_scroll_down();
void key_tasklist_scroll_end();
void key_tasklist_scroll_home();
void key_tasklist_scroll_up();
void key_tasklist_search(const char *arg);
void key_tasklist_search_next();
void key_tasklist_sort(const char *arg);
void key_tasklist_sync();
void key_tasklist_action(const task_action_type action, const char *msg_success, const char *msg_fail);
void key_tasklist_undo();
void tasklist_window();
void tasklist_print_task(int tasknum, task *this);
void tasklist_print_task_list();
int tasklist_task_action(const task_action_type action);
void tasklist_task_add();

extern bool reload;
extern bool redraw;
extern bool resize;
extern bool done;
extern char *active_filter;
extern char *searchstring;
extern config cfg;
extern FILE *logfp;
extern int cols;
extern int rows;
extern int selline;
extern int taskcount;
extern short pageoffset;
extern task *head;
extern time_t sb_timeout;
extern WINDOW *tasklist;
extern WINDOW *header;
extern WINDOW *statusbar;

#endif
