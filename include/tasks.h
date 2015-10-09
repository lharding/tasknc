/*
 * tasks.h
 * for tasknc
 * by mjheagle
 */

#ifndef _TASKS_H
#define _TASKS_H

#include <stdbool.h>
#include "common.h"

char free_task(struct task* tsk);
void free_tasks(struct task* head);
struct task* get_task_by_position(int n);
int get_task_position_by_uuid(const char* uuid);
struct task* get_tasks(char* uuid);
unsigned short get_task_id(char* uuid);
struct task* malloc_task(void);
struct task* parse_task(char* line);
void reload_task(struct task* this);
void reload_tasks(void);
void remove_char(char* str, char remove);
void set_position_by_uuid(const char* uuid);
int task_background_command(const char* cmdfmt);
void task_count(void);
int task_interactive_command(const char* cmdfmt);
bool task_match(const struct task* cur, const char* str);
void task_modify(const char* argstr);

extern FILE* logfp;
extern struct task* head;
extern bool redraw;
extern int selline;
extern int taskcount;
extern char* active_filter;

#endif

// vim: et ts=4 sw=4 sts=4
