/**
 * @file task.h
 * @author mjheagle
 * @brief taskwarrior interface
 */

#ifndef _TASK_H
#define _TASK_H

#include <time.h>

/**
 * opaque representation of task struct.
 * use the task_get_* functions to access the struct's fields.
 */
struct task;

/**
 * tasklist struct
 * contains an array of tasks and the number of tasks
 */
struct tasklist
{
        struct task **tasks;
        int ntasks;
};

/**
 * access task index
 *
 * @param t task to obtain field from
 *
 * @return task index
 */
unsigned short task_get_index(const struct task *t);

/**
 * access task urgency
 *
 * @param t task to obtain field from
 *
 * @return task urgency
 */
float task_get_urgency(const struct task *t);

/**
 * access task uuid
 *
 * @param t task to obtain field from
 *
 * @return task uuid
 */
char *task_get_uuid(const struct task *t);

/**
 * access task tags
 *
 * @param t task to obtain field from
 *
 * @return task tags
 */
char *task_get_tags(const struct task *t);

/**
 * access task start time
 *
 * @param t task to obtain field from
 *
 * @return task start
 */
time_t task_get_start(const struct task *t);

/**
 * access task end time
 *
 * @param t task to obtain field from
 *
 * @return task end
 */
time_t task_get_end(const struct task *t);

/**
 * access task entry time
 *
 * @param t task to obtain field from
 *
 * @return task entry
 */
time_t task_get_entry(const struct task *t);

/**
 * access task due time
 *
 * @param t task to obtain field from
 *
 * @return task due
 */
time_t task_get_due(const struct task *t);

/**
 * access task project
 *
 * @param t task to obtain field from
 *
 * @return task project
 */
char *task_get_project(const struct task *t);

/**
 * access task priority
 *
 * @param t task to obtain field from
 *
 * @return task priority
 */
char task_get_priority(const struct task *t);

/**
 * access task description
 *
 * @param t task to obtain field from
 *
 * @return task description
 */
char *task_get_description(const struct task *t);

/**
 * read task list with a filter
 *
 * @param filter string to be passed on the command line as the filter.
 * the string is appended to 'task export' and executed.
 *
 * @return a task list. this list is alloc'd and must be free'd
 * using the free_tasklist function.
 */
struct tasklist * get_tasks(const char *filter);

/**
 * free a task list
 *
 * @param the task list struct to be free'd
 */
void free_tasklist(struct tasklist * list);

/**
 * get task version
 *
 * @return an array of integers, containing 3 members representing the version
 * of task warrior that is being executed.  this array is alloc'd and must be
 * free'd.
 */
int * task_version();

/**
 * count number of tasks in an array
 *
 * @param tasks the array of tasks to be counted
 *
 * @return the number of tasks in the array.
 */
int count_tasks(struct task ** tasks);

/**
 * a custom version of snprintf to format task output
 *
 * @param len length of the string
 * @param format format string
 * @param task task to format
 *
 * @return the formatted task string
 */
char * task_snprintf(int len, char * format, struct task * t);

/**
 * reload task list
 *
 * @param list tasklist to reload
 * @param filter filter to use reloading tasks
 *
 * @return an indicator of succes
 */
int reload_tasklist(struct tasklist *list, const char *filter);

/**
 * complete tasks
 *
 * @param list tasklist to complete from
 * @param indexes an array of indexes of tasks to be completed
 * @param ntasks the number of indexes contained in the indexes array
 * @param filter the task filter to apply
 *
 * @return an indicator of success
 */
int task_complete(struct tasklist *list, int *indexes, const int ntasks, const char *filter);

#endif
