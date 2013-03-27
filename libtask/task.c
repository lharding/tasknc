/*
 * taskwarrior interface
 * by mjheagle
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "json.h"
#include "task.h"

#define check_free(a) { if (a != NULL) free(a); }

/* task struct definition */
struct task
{
        /* taskwarrior data */
        char *description;
        char *project;
        char *tags;
        char *uuid;
        const char priority;
        const float urgency;
        const time_t due;
        const time_t end;
        const time_t entry;
        const time_t start;
        const unsigned short index;
};

/* task fields */
enum task_fields { TASK_INDEX, TASK_UUID, TASK_TAGS, TASK_START, TASK_END, TASK_ENTRY,
        TASK_DUE, TASK_PROJECT, TASK_PRIORITY, TASK_DESCRIPTION, TASK_URGENCY, TASK_UNKNOWN };

/* simple functions for accessing task struct fields */
unsigned short task_get_index(const struct task *t) {
        return t->index;
}
float task_get_urgency(const struct task *t) {
        return t->urgency;
}
char *task_get_uuid(const struct task *t) {
        return t->uuid;
}
char *task_get_tags(const struct task *t) {
        return t->tags;
}
time_t task_get_start(const struct task *t) {
        return t->start;
}
time_t task_get_end(const struct task *t) {
        return t->end;
}
time_t task_get_entry(const struct task *t) {
        return t->entry;
}
time_t task_get_due(const struct task *t) {
        return t->due;
}
char *task_get_project(const struct task *t) {
        return t->project;
}
char task_get_priority(const struct task *t) {
        return t->priority;
}
char *task_get_description(const struct task *t) {
        return t->description;
}

/* determine a task field from a string */
enum task_fields parse_task_fields_name(const char *name) {
        if (strcmp("id", name) == 0)
                return TASK_INDEX;
        else if (strcmp("uuid", name) == 0)
                return TASK_UUID;
        else if (strcmp("tags", name) == 0)
                return TASK_TAGS;
        else if (strcmp("start", name) == 0)
                return TASK_START;
        else if (strcmp("end", name) == 0)
                return TASK_END;
        else if (strcmp("entry", name) == 0)
                return TASK_ENTRY;
        else if (strcmp("due", name) == 0)
                return TASK_DUE;
        else if (strcmp("project", name) == 0)
                return TASK_PROJECT;
        else if (strcmp("priority", name) == 0)
                return TASK_PRIORITY;
        else if (strcmp("description", name) == 0)
                return TASK_DESCRIPTION;
        else if (strcmp("urgency", name) == 0)
                return TASK_URGENCY;
        else
                return TASK_UNKNOWN;
}

/* convert a string to a time_t */
time_t strtotime(const char *timestr) {
        struct tm tmr;

        memset(&tmr, 0, sizeof(tmr));
        strptime(timestr, "%Y%m%dT%H%M%S%z", &tmr);
        return mktime(&tmr);
}

/* get task command to run */
const char *task_cmd() {
        char *taskbin = getenv("TASKCMD");
        if (taskbin != NULL)
                return taskbin;
        else
                return TASKBIN;
}

/* parse a task struct from a string */
struct task * parse_task(const char *str) {
        /* parse json */
        char ** fields = parse_json(str);
        if (fields == NULL)
                return NULL;

        /* fields for new task */
        char priority = 0;
        char *description = NULL;
        char *project = NULL;
        char *tags = NULL;
        char *uuid = NULL;
        float urgency = 0;
        unsigned short index = 0;
        time_t due = 0;
        time_t end = 0;
        time_t entry = 0;
        time_t start = 0;

        /* iterate through json fields and assign to task struct */
        char ** f;
        for (f = fields; *f != NULL; f += 2) {
                char *field = *f;
                char *value = *(f+1);

                const enum task_fields task_field = parse_task_fields_name(field);
                switch (task_field) {
                        case TASK_UUID:
                                uuid = value;
                                break;
                        case TASK_TAGS:
                                tags = value;
                                break;
                        case TASK_PROJECT:
                                project = value;
                                break;
                        case TASK_DESCRIPTION:
                                description = value;
                                break;
                        case TASK_PRIORITY:
                                priority = *value;
                                free(value);
                                break;
                        case TASK_INDEX:
                                sscanf(value, "%hd", &index);
                                free(value);
                                break;
                        case TASK_URGENCY:
                                sscanf(value, "%f", &urgency);
                                free(value);
                                break;
                        case TASK_DUE:
                                due = strtotime(value);
                                free(value);
                                break;
                        case TASK_END:
                                end = strtotime(value);
                                free(value);
                                break;
                        case TASK_ENTRY:
                                entry = strtotime(value);
                                free(value);
                                break;
                        case TASK_START:
                                start = strtotime(value);
                                free(value);
                                break;
                        case TASK_UNKNOWN:
                        default:
                                break;
                }
                free(field);
        }
        free(fields);

        /* create a local task struct */
        struct task ltask = { description, project, tags, uuid, priority, urgency,
                due, end, entry, start, index };

        /* copy the local task struct to a alloc'd counterpart */
        struct task * ntask = calloc(1, sizeof(struct task));
        memcpy(ntask, &ltask, sizeof(struct task));

        return ntask;
}

/* function to get all tasks' data */
struct tasklist * get_tasks(const char *filter) {
        /* generate command to run */
        char *cmd;
        const char *taskbin = task_cmd();
        if (filter != NULL)
                asprintf(&cmd, "%s export %s", taskbin, filter);
        else
                asprintf(&cmd, "%s export", taskbin);

        /* allocate task array */
        int ntasks = 16;
        struct task ** tasks = calloc(ntasks, sizeof(struct task *));

        /* iterate through command output */
        FILE *out = popen(cmd, "r");
        int linelen = 256;
        free(cmd);
        char *line = calloc(linelen, sizeof(char));
        int counter = 0;
        while (fgets(line, linelen-1, out) != NULL || !feof(out)) {
                /* check for longer lines */
                while (strchr(line, '\n') == NULL) {
                        char *tmp = calloc(linelen, sizeof(char));
                        if (fgets(tmp, linelen-1, out) == NULL)
                                break;
                        line = realloc(line, 2*linelen*sizeof(char));
                        strncat(line, tmp, linelen-1);
                        free(tmp);
                        linelen *= 2;
                }

                /* parse task */
                struct task * this = parse_task(line);

                /* store task in list */
                if (counter >= ntasks-1) {
                        ntasks *= 2;
                        tasks = realloc(tasks, ntasks*sizeof(struct task *));
                }
                tasks[counter] = this;
                counter++;

                memset(line, 0, linelen);
        }
        free(line);
        pclose(out);

        /* shrink task list */
        tasks = realloc(tasks, (counter+1)*sizeof(struct task *));

        /* create tasklist */
        struct tasklist *list = calloc(1, sizeof(struct tasklist));
        list->tasks = tasks;
        list->ntasks = count_tasks(tasks);

        return list;
}

/* function to get task version */
int * task_version() {
        int * version = calloc(4, sizeof(int));
        FILE *out = popen("task --version", "r");

        char *line = calloc(16, sizeof(char));
        if (fgets(line, 15, out) == NULL)
                goto errorout;

        if (sscanf(line, "%d.%d.%d", version, version+1, version+2) != 3)
                goto errorout;

        goto done;

errorout:
        free(line);
        free(version);
        version = NULL;
        goto done;

done:
        pclose(out);

        return version;
}

/* function to free an individual task */
void free_task(struct task *t ) {
        check_free(t->description);
        check_free(t->project);
        check_free(t->tags);
        check_free(t->uuid);

        free(t);
}

/* function to free all tasks */
void free_tasks(struct task ** tasks) {
        if (tasks == NULL)
                return;

        struct task ** h;
        for (h = tasks; *h != NULL; h++)
                free_task(*h);
        free(tasks);
}

/* function to free a tasklist */
void free_tasklist(struct tasklist * list) {
        free_tasks(list->tasks);
        free(list);
}

/* function to count number of tasks */
int count_tasks(struct task ** tasks) {
        int ntasks = 0;
        struct task ** h;

        for (h = tasks; *h != 0; h++)
                ntasks++;

        return ntasks;
}

/* task snprintf implementation */
char * task_snprintf(int len, char * format, struct task * t) {
        char * ret = calloc(len+1, sizeof(char));
        int pos = 0; /* position in output string */
        char * fmtstart = NULL;
        char fmttype = 0;
        union {
                int d;
                char * s;
                char c;
                float f;
        } arg;

        while (*format != 0 && pos < len) {
                switch (*format) {
                        case '%':
                                fmtstart = format;
                                break;
                        case 'n': /* index */
                                fmttype = 'd';
                                arg.d = task_get_index(t);
                                break;
                        case 'd': /* description */
                                fmttype = 's';
                                arg.s = task_get_description(t);
                                break;
                        case 'p': /* project */
                                fmttype = 's';
                                arg.s = task_get_project(t);
                                break;
                        case 'r': /* priority */
                                fmttype = 'c';
                                arg.c = task_get_priority(t);
                                break;
                        case 'u': /* uuid */
                                fmttype = 's';
                                arg.s = task_get_uuid(t);
                                break;
                        case 't': /* tags */
                                fmttype = 's';
                                arg.s = task_get_tags(t);
                                break;
                        case 'g': /* urgency */
                                fmttype = 'f';
                                arg.f = task_get_urgency(t);
                                break;
                        default:
                                if (!fmtstart) {
                                        ret[pos] = *format;
                                        pos++;
                                }
                                break;
                }

                /* handle format string */
                if (fmttype) {
                        char * fmt = calloc(format-fmtstart+2, sizeof(char));
                        strncpy(fmt, fmtstart, format-fmtstart+1);
                        fmt[format-fmtstart] = fmttype;
                        /* printf("fmt: %s\n", fmt); */
                        char * field = NULL;
                        switch (fmttype) {
                                case 'd':
                                        asprintf(&field, fmt, arg.d);
                                        break;
                                case 's':
                                        if (arg.s == NULL)
                                                arg.s = "";
                                        asprintf(&field, fmt, arg.s);
                                        break;
                                case 'c':
                                        asprintf(&field, fmt, arg.c);
                                        break;
                                case 'f':
                                        asprintf(&field, fmt, arg.f);
                                        break;
                                default:
                                        break;
                        }

                        if (field) {
                                int flen = strlen(field);
                                if (pos+flen >= len) {
                                        flen = len-pos;
                                }
                                strncat(ret+pos, field, flen);
                                pos += flen;
                                free(field);
                        }

                        free(fmt);
                        fmttype = 0;
                        fmtstart = NULL;
                }

                /* move to next char */
                format++;
        }

        return ret;
}

/* run a command in the background */
int task_background_command(const char *cmdfmt, const char *uuids) {
        char *cmdstr, *line;
        FILE *cmd;
        int ret;

        /* build command */
        asprintf(&cmdstr, cmdfmt, task_cmd(), uuids);
        cmdstr = realloc(cmdstr, (strlen(cmdstr)+6)*sizeof(char));
        strcat(cmdstr, " 2>&1");

        /* run command in background */
        cmd = popen(cmdstr, "r");
        free(cmdstr);
        while (!feof(cmd)) {
                ret = fscanf(cmd, "%m[^\n]*", &line);
                if (ret == 1)
                        free(line);
                else
                        break;
        }
        ret = pclose(cmd);

        /* log command return value */
        if (WEXITSTATUS(ret) == 0 || WEXITSTATUS(ret) == 128+SIGPIPE)
                ret = 0;
        else
                ret = WEXITSTATUS(ret);

        return ret;
}

/* generate a string of uuids from a tasklist and array of indices */
char *cat_uuids(struct tasklist *list, int *indexes, const int ntasks) {
        char *uuids = calloc(ntasks*(UUIDLEN+1), sizeof(char));
        int n;
        for (n = 0; n < ntasks; n++) {
                if (n>0)
                        strcat(uuids, " ");
                strcat(uuids, task_get_uuid(list->tasks[indexes[n]]));
        }

        return uuids;
}

/* reload task list */
int reload_tasklist(struct tasklist *list, const char *filter) {
        /* get new tasklist */
        struct tasklist *new = get_tasks(filter);
        if (new == NULL)
                return 1;

        /* replace fields and free new tasklist */
        list->tasks = new->tasks;
        list->ntasks = new->ntasks;
        free(new);

        return 0;
}

/* complete a task */
int task_complete(struct tasklist *list, int *indexes, const int ntasks, const char *filter) {
        /* generate uuids string */
        char *uuids = cat_uuids(list, indexes, ntasks);

        /* run command */
        int ret = task_background_command("%s %s done", uuids);
        free(uuids);

        /* return on failure */
        if (ret != 0)
                return ret;

        /* reload task list */
        if (reload_tasklist(list, filter) != 0)
                return 99;
        return ret;
}

/* delete a task */
int task_delete(struct tasklist *list, int *indexes, const int ntasks, const char *filter) {
        /* generate uuids string */
        char *uuids = cat_uuids(list, indexes, ntasks);

        /* run command */
        int ret = task_background_command("%s %s delete", uuids);
        free(uuids);

        /* return on failure */
        if (ret != 0)
                return ret;

        /* reload task list */
        if (reload_tasklist(list, filter) != 0)
                return 99;
        return ret;
}

/* undo the last task action */
int task_undo(struct tasklist *list, const char *filter) {
        /* run command */
        int ret = task_background_command("%s %s undo", "");

        /* return on failure */
        if (ret != 0)
                return ret;

        /* reload task list */
        if (reload_tasklist(list, filter) != 0)
                return 99;
        return ret;
}
