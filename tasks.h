/*
 * tasks.h
 * for tasknc
 * by mjheagle
 */

#ifndef _TASKS_H
#define _TASKS_H

#include "common.h"

char free_task(task *);
void free_tasks(task *);
task *get_task_by_position(int);
task *get_tasks(char *);
unsigned short get_task_id(char *);
task *malloc_task(void);
task *parse_task(char *);
void reload_task(task *);
void reload_tasks();
void sort_wrapper(task *);

extern FILE *logfp;
extern task *head;
extern int taskcount;
extern int totaltaskcount;
extern char *active_filter;

#endif
