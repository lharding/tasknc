/*
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <curses.h>
#include <signal.h>
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
char *utc_date(unsigned int);
void nc_main(task *);
void nc_colors();
void color_line(char, char, char);
void print_task_list(task *, short, short, short);
void nc_end(int);
char max_project_length(task *);
char task_count(task *);
char *pad_string(char*, int, int, int, char);
/* }}} */

/* main {{{ */
int
main(int argc, char **argv)
{
        task *head, *cur;

        /* build task list */
        head = get_tasks();

        /* test uusing stdio */
        cur = head;
        while (cur!=NULL)
        {
                /* print_task(cur); */
                cur = cur->next;
        }

        /* run ncurses */
        nc_main(head);
        nc_end(0);


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

/* malloc_task {{{ */
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

/* utc_date {{{ */
char *
utc_date(unsigned int timeint)
{
        /* convert a utc time uint to a string */
        struct tm tmr;
        char *timestr, *srcstr;

        srcstr = malloc(16*sizeof(char));
        timestr = malloc(TIMELENGTH*sizeof(char));
        sprintf(srcstr, "%d", timeint);
        strptime(srcstr, "%s", &tmr);
        free(srcstr);
        strftime(timestr, TIMELENGTH, "%F %H:%M:%S", &tmr);

        return timestr;
}
/* }}} */

/* print_task {{{ */
void
print_task(task *tsk)
{
        /* print a task to stdio */
        char *timestr;

        printf("==============================\n");
        printf("uuid: %s\n", tsk->uuid);
        printf("tags: %s\n", tsk->tags);
        timestr = utc_date(tsk->entry);
        printf("entry: %s\n", timestr);
        free(timestr);
        printf("due: %d\n", tsk->due);
        printf("project: %s\n", tsk->project);
        printf("priority: %c\n", tsk->priority);
        printf("description: %s\n", tsk->description);
        puts("\n");
}
/* }}} */

/* nc_main {{{ */
void
nc_main(task *head)
{
        /* ncurses main function */
        WINDOW *stdscr;
        int c;
        int size[2];
        char *pos;
        short selline = 0;
        char taskcount;
        short projlen = max_project_length(head);
        short desclen;

        /* initialize ncurses */
        puts("starting ncurses...");
        stdscr = signal(SIGINT, nc_end);
        if ((stdscr = initscr()) == NULL ) {
            fprintf(stderr, "Error initialising ncurses.\n");
            exit(EXIT_FAILURE);
        }
        keypad(stdscr, TRUE);   /* enable keyboard mapping */
        nonl();                 /* tell curses not to do NL->CR/NL on output */
        cbreak();               /* take input chars one at a time, no wait for \n */
        noecho();               /* dont echo input */
        nc_colors();            /* initialize colors */

        /* set variables */
        getmaxyx(stdscr, size[1], size[0]);
        desclen = size[0]-projlen-1;
        taskcount = task_count(head);

        /* print main screen */
        color_line(0, size[0], 1);
        char *title = pad_string("task ncurses - by mjheagle", size[0], 0, 0, 'l');
        mvaddstr(0, 0, title);
        free(title);
        pos = malloc(8*sizeof(char));
        sprintf(pos, "(%d, %d)", size[0], size[1]);
        mvaddstr(0, 30, pos);
        free(pos);
        attrset(COLOR_PAIR(0));
        print_task_list(head, selline, projlen, desclen);
        refresh();

        /* main loop */
        while (1)
        {
                char done = 0;
                char redraw = 0;

                c = getch();

                switch (c)
                {
                        case 'k':
                                if (selline>0)
                                {
                                        selline--;
                                        redraw = 1;
                                }
                                break;
                        case 'j':
                                if (selline<taskcount-1)
                                {
                                        selline++;
                                        redraw = 1;
                                }
                                break;
                        case 'q':
                                done = 1;
                                break;
                        default:
                                break;
                }
                char cstr[] = {c, NULL};
                mvaddstr(0, 40, cstr);
                if (done==1)
                        break;
                if (redraw==1)
                        print_task_list(head, selline, projlen, desclen);
                        refresh();
        }

        delwin(stdscr);
}
/* }}} */

/* nc_colors {{{ */
void
nc_colors()
{
        if (has_colors())
        {
                start_color();
                use_default_colors();
                init_pair(1, COLOR_BLUE,        COLOR_BLACK);   /* title bar */
                init_pair(2, COLOR_GREEN,       -1);            /* project */
                init_pair(3, COLOR_CYAN,        -1);            /* description */
                init_pair(4, COLOR_YELLOW,      -1);
                init_pair(5, COLOR_BLACK,       COLOR_GREEN);
                init_pair(6, COLOR_BLACK,       COLOR_CYAN);
                init_pair(7, COLOR_BLACK,       COLOR_YELLOW);
        }
}
/* }}} */

/* nc_end {{{ */
void
nc_end(int sig)
{
        endwin();
        /* free all structs here */
        exit(0);
}
/* }}} */

/* color_line {{{ */
void
color_line(char lineno, char width, char color)
{
        char *str;
        char i;

        str = malloc((width+1)*sizeof(char));
        for (i=0; i<width; i++)
                str[i] = ' ';
        attrset(COLOR_PAIR(color));
        mvaddstr(lineno, 0, str);
}
/* }}} */

/* print_task_list {{{ */
void
print_task_list(task *head, short selected, short projlen, short desclen)
{
        task *cur;
        short counter = 0;
        char sel = 0;
        char *bufstr;

        cur = head;
        while (cur!=NULL)
        {
                if (counter==selected)
                        sel = 1;
                else
                        sel = 0;
                attrset(COLOR_PAIR(2+3*sel));
                bufstr = pad_string(cur->project, projlen, 0, 1, 'r');
                mvaddstr(counter+1, 0, bufstr);
                free(bufstr);
                attrset(COLOR_PAIR(3+3*sel));
                bufstr = pad_string(cur->description, desclen, 0, 0, 'l');
                mvaddstr(counter+1, projlen+1, bufstr);
                free(bufstr);
                counter++;
                cur = cur->next;
        }
}
/* }}} */

/* max_project_length {{{ */
char 
max_project_length(task *head)
{
        char len = 0;
        task *cur;

        cur = head;
        while (cur!=NULL)
        {
                char l = strlen(cur->project);
                if (l>len)
                        len = l;
                cur = cur->next;
        }

        return len;
}
/* }}} */

/* task_count {{{ */
char
task_count(task *head)
{
        char count = 0;
        task *cur;

        cur = head;
        while (cur!=NULL)
        {
                count++;
                cur = cur->next;
        }

        return count;
}
/* }}} */

/* pad_string {{{ */
char *
pad_string(char *str, int length, int lpad, int rpad, char align)
{
        /* function to add padding to strings and align them with spaces */
        char *ft;
        char *ret;

        /* handle left alignment */
        if (align=='l')
        {
                int slen = strlen(str);
                rpad = rpad + length - slen;
                length = slen;
        }

        /* generate format strings and return value */
        ret = malloc((length+lpad+rpad+1)*sizeof(char));
        ft = malloc(16*sizeof(char));
        if (lpad>0 && rpad>0)
        {
                sprintf(ft, "%%%ds%%%ds%%%ds", lpad, length, rpad);
                sprintf(ret, ft, " ", str, " ");
        }
        else if (lpad>0)
        {
                sprintf(ft, "%%%ds%%%ds", lpad, length);
                sprintf(ret, ft, " ", str);
        }
        else if (rpad>0)
        {
                sprintf(ft, "%%%ds%%%ds", length, rpad);
                sprintf(ret, ft, str, " ");
        }
        else
        {
                sprintf(ft, "%%%ds", length);
                sprintf(ret, ft, str);
        }
        free(ft);

        return ret;
}
/* }}} */
