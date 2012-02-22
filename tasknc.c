/*
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"

/* struct definitions {{{ */
struct _task
{
        char *uuid;
        char *tags;
        unsigned int start;
        unsigned int end;
        unsigned int entry;
        unsigned int due;
        char *project;
        char priority;
        char *description;
        struct _task *next;
};
typedef struct _task task;
/* }}} */

/* function declarations {{{ */
task *get_tasks();
task *malloc_task();
task *parse_task(char *);
char free_task(task *);
void print_task(task *);
/* }}} */

/* main {{{ */
int
main(int argc, char **argv)
{
        task *head, *cur;

        head = get_tasks();
        cur = head;
        while (cur!=NULL)
        {
                print_task(cur);
                cur = cur->next;
        }

        // done
        return 0;
}
/* }}} */

/* get_tasks {{{ */
task *
get_tasks()
{
        FILE *cmd;
        char line[TOTALLENGTH];
        short counter = 0;
        task *head, *last;

        // run command
        cmd = popen("task export.csv status:pending", "r");

        // parse output
        last = NULL;
        head = NULL;
        while (fgets(line, sizeof(line)-1, cmd) != NULL)
        {
                task *this;
                if (counter>0)
                        this = parse_task(line);
                if (counter==1)
                        head = this;
                if (counter>1)
                        last->next = this;
                last = this;
                counter++;
        }

        return head;
}
/* }}} */

/* malloc task {{{ */
task *
malloc_task()
{
        task *tsk = malloc(sizeof(task));

        tsk->uuid = malloc(UUIDLENGTH*sizeof(char));
        tsk->tags = malloc(TAGSLENGTH*sizeof(char));
        tsk->project = malloc(PROJECTLENGTH*sizeof(char));
        tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
        tsk->next = NULL;

        return tsk;
}
/* }}} */

/* parse_task {{{ */
task *
parse_task(char *line)
{
        task *tsk = malloc_task();
        char *token, *lastchar;
        short counter = 0;

        token = strtok(line, ",");
        while (token != NULL)
        {
                int ret;
                lastchar = token;
                while (*(--lastchar) == ',')
                        counter++;
                switch (counter)
                {
                        case 0: // uuid
                                ret = sscanf(token, "'%[^\']'", tsk->uuid);
                                if (ret==0)
                                {
                                        free_task(tsk);
                                        return NULL;
                                }
                                break;
                        case 1: // status
                                break;
                        case 2: // tags
                                ret = sscanf(token, "'%[^\']'", tsk->tags);
                                if (ret==0)
                                {
                                        free(tsk->tags);
                                        tsk->tags = NULL;
                                }
                                break;
                        case 3: // entry
                                ret = sscanf(token, "%d", &tsk->entry);
                                if (ret==0)
                                {
                                        free_task(tsk);
                                        return NULL;
                                }
                                break;
                        case 4: // start
                                ret = sscanf(token, "%d", &tsk->start);
                                break;
                        case 5: // due
                                ret = sscanf(token, "%d", &tsk->due);
                                break;
                        case 6: // recur
                                break;
                        case 7: // end
                                ret = sscanf(token, "%d", &tsk->end);
                                break;
                        case 8: // project
                                ret = sscanf(token, "'%[^\']'", tsk->project);
                                if (ret==0)
                                {
                                        free(tsk->project);
                                        tsk->project = NULL;
                                }
                                break;
                        case 9: // priority
                                ret = sscanf(token, "'%c'", &tsk->priority);
                                break;
                        case 10: // fg
                                break;
                        case 11: // bg
                                break;
                        case 12: // description
                                ret = sscanf(token, "'%[^\']'", tsk->description);
                                break;
                        default:
                                break;
                }
                token = strtok(NULL, ",");
                counter++;
        }

        return tsk;
}
/* }}} */

/* free_task {{{ */
char
free_task(task *tsk)
{
        /* free the memory allocated to a task */
        char ret = 0;

        free(tsk->uuid);
        if (tsk->tags!=NULL)
                free(tsk->tags);
        if (tsk->project!=NULL)
                free(tsk->project);
        if (tsk->description!=NULL)
                free(tsk->description);
        free(tsk);

        return ret;
}
/* }}} */

/* print_task {{{ */
void
print_task(task *tsk)
{
        /* print a task to stdio */
        printf("==============================\n");
        printf("uuid: %s\n", tsk->uuid);
        printf("tags: %s\n", tsk->tags);
        printf("entry: %d\n", tsk->entry);
        printf("due: %d\n", tsk->due);
        printf("project: %s\n", tsk->project);
        printf("priority: %c\n", tsk->priority);
        printf("description: %s\n", tsk->description);
        puts("\n");
}
/* }}} */
