/*
 * tasks.h
 * for tasknc
 * by mjheagle
 *
 * vim: noet ts=4 sw=4 sts=4
 */

#ifndef _TASKS_H
#define _TASKS_H

#include <stdbool.h>
#include "common.h"

char free_task(task *);
void free_tasks(task *);
task *get_task_by_position(int);
int get_task_position_by_uuid(const char *uuid);
task *get_tasks(char *);
unsigned short get_task_id(char *);
task *malloc_task(void);
task *parse_task(char *);
void reload_task(task *);
void reload_tasks();
void remove_char(char *, char);
void set_position_by_uuid(const char *uuid);
void sort_wrapper(task *);
int task_background_command(const char *);
void task_count();
int task_interactive_command(const char *);
bool task_match(const task *, const char *);
void task_modify(const char *);

extern FILE *logfp;
extern task *head;
extern bool redraw;
extern int selline;
extern int taskcount;
extern char *active_filter;

#endif
