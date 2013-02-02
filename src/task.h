/*
 * taskwarrior interface
 * by mjheagle
 */

#ifndef _TASKNC_TASK_H
#define _TASKNC_TASK_H

#include <time.h>

/* opaque representation of task struct */
struct task;

/* functions for accessing task fields */
unsigned short task_get_index(const struct task *);
char *task_get_uuid(const struct task *);
char *task_get_tags(const struct task *);
time_t task_get_start(const struct task *);
time_t task_get_end(const struct task *);
time_t task_get_entry(const struct task *);
time_t task_get_due(const struct task *);
char *task_get_project(const struct task *);
char task_get_priority(const struct task *);
char *task_get_description(const struct task *);

/*
 * read task list with a filter
 * filter is passed on command line to task
 */
struct task ** get_tasks(const char *);

/*
 * free array of tasks
 */
void free_tasks(struct task **);

/* get task version */
int * task_version();

#endif
