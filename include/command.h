/*
 * command.h
 * by mjheagle
 */

#ifndef _COMMAND_H
#define _COMMAND_H

#include <curses.h>
#include <stdbool.h>
#include "common.h"

void handle_command(char* cmdstr);
void run_command_bind(char* args);
void run_command_color(char* args);
void run_command_unbind(char* argstr);
void run_command_set(char* args);
void run_command_show(const char* arg);
void run_command_source(const char* filepath);
void run_command_source_cmd(const char* cmdstr);
void strip_quotes(char** strptr, bool needsfree);

extern bool done;
extern bool redraw;
extern bool reload;
extern struct task* head;
extern FILE* logfp;
extern WINDOW* pager;
extern WINDOW* tasklist;

#endif

// vim: et ts=4 sw=4 sts=4
