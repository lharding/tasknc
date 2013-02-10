/*
 * debug main during restructuring of tasknc
 * by mjheagle
 */

#include <stdio.h>
#include <stdlib.h>
#include "configure.h"
#include "sort.h"
#include "task.h"
#include "tasklist.h"

int main() {
        /* configure */
        struct config *conf = default_config();
        printf("initial nc_timeout: %d\n", conf_get_nc_timeout(conf));
        /* config_parse_string(conf, "set nc_timeout 3000"); */
        FILE *fd = fopen("config", "r");
        if (fd)
                config_parse_file(conf, fd);
        fclose(fd);
        printf("final nc_timeout: %d\n", conf_get_nc_timeout(conf));

        /* get tasks and print */
        struct task ** tasks = get_tasks("status:pending");
        sort_tasks(tasks, 0, "N");
        struct task ** t;
        for (t = tasks; *t != 0; t++) {
                printf("%d: '%s' (%s)\n", task_get_index(*t), task_get_description(*t), task_get_project(*t));
        }

        /* run tasklist window, then free tasks */
        tasklist_window(tasks, conf);
        free_tasks(tasks);

        /* get task version */
        int * version = conf_get_version(conf);
        if (version != NULL)
                printf("task version: %d.%d.%d\n", version[0], version[1], version[2]);

        /* free configuration */
        free_config(conf);

        return 0;
}
