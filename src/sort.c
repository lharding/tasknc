/*
 * sorting for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sort.h"

/* function to compare two strings, but check for nulls first */
int nullstrcmp(const char *a, const char *b) {
        if (a == NULL && b == NULL)
                return 0;
        else if (a == NULL)
                return 1;
        else if (b == NULL)
                return -1;

        return strcmp(a, b);
}

/* function to map a priority to an integer */
int priority_to_int(const char p) {
        switch (p) {
                case 'H':
                case 'h':
                        return 3;
                        break;
                case 'M':
                case 'm':
                        return 2;
                        break;
                case 'L':
                case 'l':
                        return 1;
                        break;
                default:
                        return 0;
                        break;
        }
}

/* recursive function to compare two tasks */
int task_compare(const void * t1, const void * t2, void * sortorder) {
        const struct task * a = *(const struct task **)t1;
        const struct task * b = *(const struct task **)t2;

        char mode = *(const char *)sortorder;
        bool invert = false;

        if (mode >= 'A' && mode <= 'Z') {
                mode += 32;
                invert = true;
        }

        /* determine sort function and run it */
        int ret = 0;
        switch (mode) {
                case 0: // end of string
                        return 0;
                        break;
                case 'n': // index
                        ret = task_get_index(a) - task_get_index(b);
                        break;
                case 'p': // project
                        ret = nullstrcmp(task_get_project(a), task_get_project(b));
                        break;
                case 'd': // due date
                        ret = task_get_due(a) - task_get_due(b);
                        break;
                case 'r': // priority
                        ret = priority_to_int(task_get_priority(a)) - priority_to_int(task_get_priority(b));
                        break;
                case 'u': // uuid
                        ret = strcmp(task_get_uuid(a), task_get_uuid(b));
                        break;
                default:
                        break;
        }

        /* invert return if necessary */
        if (invert)
                ret = -1*ret;

        /* call next sort mode if necessary */
        if (ret != 0)
                return ret;

        return task_compare(t1, t2, sortorder+1);
}

/* function to sort tasks in a specified order */
void sort_tasks(struct task ** tasks, int ntasks, const char * sortmode) {
        /* count tasks if necessary */
        if (ntasks == 0) {
                struct task ** h;
                for (h = tasks; *h != 0; h++)
                        ntasks++;
        }

        /* set sort order and run qsort */
        qsort_r(tasks, ntasks, sizeof(struct task *), task_compare, (void *)sortmode);
}
