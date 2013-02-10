/*
 * debug main during restructuring of tasknc
 * by mjheagle
 */

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "configure.h"
#include "sort.h"
#include "task.h"
#include "tasklist.h"

/* macros */
#define clean(t, c) { free_config(c); free_tasks(t); }

/* local functions */
typedef int (*action)(struct task **, struct config *);
int version(struct task ** tasks, struct config * conf);

int main(int argc, char ** argv) {
        /* initialize */
        struct config *conf = default_config();
        struct task ** tasks = NULL;
        action run = NULL;
        bool need_tasks = false;
        bool need_conf = false;
        char *filter = "status:pending";

        /* determine which action to take */
        static struct option long_opt[] = {
                {"version",     no_argument,    0, 'v'},
                {0,             0,              0, 0}
        };
        int opt_index = 0;
        int c;
        while ((c = getopt_long(argc, argv, "v", long_opt, &opt_index)) != -1) {
                switch (c) {
                        case 'v':
                                run = version;
                                break;
                        case '?':
                        default:
                                printf("unknown arg: '%c'\n", c);
                                printf("TODO: implement help function here\n");
                                return 1;
                                break;
                }
        }

        /* get necessary variables */
        if (need_tasks)
                tasks = get_tasks(filter);

        /* run function */
        if (run)
                return run(tasks, conf);
        else {
                printf("no action to run\n");
                return 1;
        }
}

/* get task version */
int version(struct task ** tasks, struct config * conf) {
        int * version = conf_get_version(conf);
        if (version != NULL)
                printf("task version: %d.%d.%d\n", version[0], version[1], version[2]);

        clean(tasks, conf);

        return 0;
}
