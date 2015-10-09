/*
 * view.h
 * for tasknc
 * by mjheagle
 */


#ifndef _VIEW_H
#define _VIEW_H

#include <stdbool.h>
#include "common.h"

/**
 * line struct (basic string vector struct)
 * str  - the string contained
 * next - the next line struct
 */
struct line {
    char* str;
    struct line* next;
};

void free_lines(struct line* head);
void help_window(void);
void key_pager_close(void);
void key_pager_scroll_down(void);
void key_pager_scroll_end(void);
void key_pager_scroll_home(void);
void key_pager_scroll_up(void);
void pager_command(const char* cmdstr,
                   const char* title,
                   const bool fullscreen,
                   const int head_skip,
                   const int tail_skip);
void view_stats(void);
void view_task(struct task* this);

extern bool redraw;
extern config cfg;
extern FILE* logfp;
extern int cols;
extern int rows;
extern WINDOW* pager;
extern WINDOW* statusbar;

#endif

// vim: et ts=4 sw=4 sts=4
