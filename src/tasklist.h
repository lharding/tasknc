/**
 * @file tasklist.h
 * @author mjheagle
 * @brief tasklist window for tasknc
 */

#ifndef _TASKNC_TASKLIST_H
#define _TASKNC_TASKLIST_H

#include "task.h"

/**
 * display an array of tasks in a ncurses window
 *
 * @param tasks the array of tasks to be displayed in the window
 */
void tasklist_window(struct task ** tasks);

#endif
