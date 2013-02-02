/**
 * @file sort.h
 * @author mjheagle
 * @brief sorting for tasknc
 */

#ifndef _TASKNC_SORT_H
#define _TASKNC_SORT_H

#include "task.h"

/**
 * sort an array of tasks
 *
 * @param tasks the array of tasks to be sorted
 * @param ntasks the number of tasks to be sorted.  if this is zero, the number
 * of tasks in the array will be counted.
 * @param sortmode a string containing the order of comparisons between tasks
 * when sorting.  a capital letter indicates to invert the order of the sort.
 * the following letters correspond to sorting by the listed fields:
 *  - n: index
 *  - p: project
 *  - d: due date
 *  - r: priority
 *  - u: uuid
 *
 *  quicksort is used for ordering, thus this sort is not stable.  it is
 *  recommended that a sort with unique values is placed last, such as index or
 *  uuid.
 */
void sort_tasks(struct task ** tasks, int ntasks, const char * sortmode);

#endif
