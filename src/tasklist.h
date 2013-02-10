/**
 * @file tasklist.h
 * @author mjheagle
 * @brief tasklist window for tasknc
 */

#ifndef _TASKNC_TASKLIST_H
#define _TASKNC_TASKLIST_H

#include "configure.h"
#include "task.h"

/**
 * display an array of tasks in a ncurses window
 *
 * @param tasks the array of tasks to be displayed in the window
 * @param conf configuration struct pointer
 *
 * @return indicator of success
 */
int tasklist_window(struct task ** tasks, struct config * conf);

#endif
