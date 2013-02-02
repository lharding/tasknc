/*
 * debug main during restructuring of tasknc
 * by mjheagle
 */

#include <stdio.h>
#include <stdlib.h>
#include "sort.h"
#include "task.h"
#include "tasklist.h"

int main() {
        struct task ** tasks = get_tasks("status:pending");
        sort_tasks(tasks, 0, "N");
        struct task ** t;
        for (t = tasks; *t != 0; t++) {
                printf("%d: '%s' (%s)\n", task_get_index(*t), task_get_description(*t), task_get_project(*t));
        }
        tasklist_window(tasks);
        free_tasks(tasks);

        int * version = task_version();
        if (version != NULL) {
                printf("task version: %d.%d.%d\n", version[0], version[1], version[2]);
                free(version);
        }

        return 0;
}
