/*
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 */

#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include <time.h>
#include "config.h"

/* struct definitions {{{ */
typedef struct _task
{
        unsigned short index;
        char *uuid;
        char *tags;
        unsigned int start;
        unsigned int end;
        unsigned int entry;
        unsigned int due;
        char *project;
        char priority;
        char *description;
        struct _task *prev;
        struct _task *next;
} task;
/* }}} */

/* function declarations {{{ */
task *get_tasks();
task *malloc_task();
task *parse_task(const char *);
char free_task(task *);
void free_tasks(task *);
char *utc_date(const unsigned int);
void nc_main(task *);
void nc_colors();
void print_task_list(const task *, const short, const short, const short, const short);
void nc_end(int);
char max_project_length(const task *);
char task_count(const task *);
char *pad_string(char *, int, const int, int, const char);
void task_action(task *, const short, const char);
void wipe_screen(const short, const int[2]);
void reload_tasks(task **);
void check_curs_pos(short *, const char);
void swap_tasks(task *, task *);
void logmsg(const char *, const char);
char *strip_quotes(char *);
void task_add(const char);
void print_title(const int);
void help();
void print_version();
void sort_wrapper(task *);
void sort_tasks(task *, task *);
char compare_tasks(const task *, const task *);
/* }}} */

/* global variables {{{ */
char loglvl = 0;
/* }}} */

/* main {{{ */
int
main(int argc, char **argv)
{
        /* declare variables */
        task *head;
        int c;

        /* handle arguments */
        while ((c = getopt(argc, argv, "l:hv")) != -1)
        {
                switch (c)
                {
                        case 'l':
                                loglvl = (char) atoi(optarg);
                                printf("loglevel: %d\n", (int)loglvl);
                                break;
                        case 'v':
                                print_version();
                                return 0;
                                break;
                        case 'h':
                        case '?':
                                help();
                                return 0;
                                break;
                        default:
                                return 1;
                }
        }

        /* build task list */
        head = get_tasks();
        sort_wrapper(head);

        /* run ncurses */
        logmsg("running gui", 1);
        nc_main(head);
        nc_end(0);

        /* done */
        free_tasks(head);
        logmsg("exiting", 1);
        return 0;
}
/* }}} */

/* help {{{ */
void
help()
{
        /* print a list of options and program info */
        print_version();
        puts("\noptions:");
        puts("  -l [value]: set log level");
        puts("  -h: print this help message");
        puts("  -v: print the version of tasknc");
}
/* }}} */

/* print_version {{{ */
void
print_version()
{
        /* print info about the currently running program */
        printf("%s v%s by %s\n", NAME, VERSION, AUTHOR);
}
/* }}} */

/* get_tasks {{{ */
task *
get_tasks()
{
        FILE *cmd;
        char line[TOTALLENGTH];
        unsigned short counter = 0;
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
                {
                        this = parse_task(line);
                        this->index = counter;
                        this->prev = last;
                }
                if (counter==1)
                        head = this;
                if (counter>1)
                        last->next = this;
                last = this;
                counter++;
        }
        pclose(cmd);

        return head;
}
/* }}} */

/* malloc_task {{{ */
task *
malloc_task()
{
        /* allocate memory for a new task 
         * and initialize values where ncessary 
         */
        task *tsk = malloc(sizeof(task));

        tsk->uuid = NULL;
        tsk->tags = NULL;
        tsk->project = NULL;
        tsk->description = NULL;
        tsk->next = NULL;
        tsk->start = 0;
        tsk->due = 0;
        tsk->end = 0;
        tsk->entry = 0;
        tsk->index = 0;
        tsk->priority = 0;

        return tsk;
}
/* }}} */

/* parse_task {{{ */
task *
parse_task(const char *line)
{
        task *tsk = malloc_task();
        char *token, *lastchar;
        short counter = 0;

        token = strtok(line, ",");
        while (token != NULL)
        {
                int tmp;
                lastchar = token;
                while (*(--lastchar) == ',')
                        counter++;
                switch (counter)
                {
                        case 0: // uuid
                                tsk->uuid = malloc(UUIDLENGTH*sizeof(char));
                                token = strip_quotes(token);
                                sprintf(tsk->uuid, "%s", token);
                                break;
                        case 1: // status
                                break;
                        case 2: // tags
                                tsk->tags = malloc(TAGSLENGTH*sizeof(char));
                                token = strip_quotes(token);
                                sprintf(tsk->tags, "%s", token);
                                break;
                        case 3: // entry
                                tmp = sscanf(token, "%d", &tsk->entry);
                                if (tmp==0)
                                {
                                        free_task(tsk);
                                        return NULL;
                                }
                                break;
                        case 4: // start
                                tmp = sscanf(token, "%d", &tsk->start);
                                break;
                        case 5: // due
                                tmp = sscanf(token, "%d", &tsk->due);
                                break;
                        case 6: // recur
                                break;
                        case 7: // end
                                tmp = sscanf(token, "%d", &tsk->end);
                                break;
                        case 8: // project
                                tsk->project = malloc(PROJECTLENGTH*sizeof(char));
                                token = strip_quotes(token);
                                sprintf(tsk->project, "%s", token);
                                break;
                        case 9: // priority
                                tmp = sscanf(token, "'%c'", &tsk->priority);
                                break;
                        case 10: // fg
                                break;
                        case 11: // bg
                                break;
                        case 12: // description
                                tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
                                token = strip_quotes(token);
                                sprintf(tsk->description, "%s", token);
                                break;
                        default:
                                break;
                }
                token = strtok(NULL, ",");
                counter++;
        }
        
        if (tsk->description==NULL || tsk->entry==0 || tsk->uuid==NULL)
                return NULL;

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

/* free_tasks {{{ */
void
free_tasks(task *head)
{
        /* free the task stack */
        task *cur, *next;
        
        cur = head;
        while (cur!=NULL)
        {
                next = cur->next;
                free_task(cur);
                cur = next;
        }
}
/* }}} */

/* utc_date {{{ */
char *
utc_date(const unsigned int timeint)
{
        /* convert a utc time uint to a string */
        struct tm tmr;
        char *timestr, *srcstr;

        srcstr = malloc(16*sizeof(char));
        timestr = malloc(TIMELENGTH*sizeof(char));
        sprintf(srcstr, "%d", timeint);
        strptime(srcstr, "%s", &tmr);
        free(srcstr);
        strftime(timestr, TIMELENGTH, "%F", &tmr);

        return timestr;
}
/* }}} */

/* nc_main {{{ */
void
nc_main(task *head)
{
        /* ncurses main function */
        WINDOW *stdscr;
        int c, tmp, size[2], oldsize[2];
        short selline = 0;
        char taskcount;
        short projlen = max_project_length(head);
        short desclen;
        const short datelen = DATELENGTH;

        /* initialize ncurses */
        puts("starting ncurses...");
        signal(SIGINT, nc_end);
        signal(SIGSEGV, nc_end);
        if ((stdscr = initscr()) == NULL ) {
            fprintf(stderr, "Error initialising ncurses.\n");
            exit(EXIT_FAILURE);
        }
        keypad(stdscr, TRUE);   /* enable keyboard mapping */
        nonl();                 /* tell curses not to do NL->CR/NL on output */
        cbreak();               /* take input chars one at a time, no wait for \n */
        noecho();               /* dont echo input */
        nc_colors();            /* initialize colors */
        curs_set(0);            /* set cursor invisible */

        /* print main screen */
        getmaxyx(stdscr, oldsize[1], oldsize[0]);
        desclen = oldsize[0]-projlen-1-datelen;
        taskcount = task_count(head);
        print_title(oldsize[0]);
        attrset(COLOR_PAIR(0));
        print_task_list(head, selline, projlen, desclen, datelen);
        refresh();

        /* main loop */
        while (1)
        {
                char done = 0;
                char redraw = 0;
                getmaxyx(stdscr, size[1], size[0]);
                if (size[0]!=oldsize[0] || size[1]!=oldsize[1])
                        redraw = 1;
                for (tmp=0; tmp<2; tmp++)
                        oldsize[tmp] = size[tmp];

                c = getch();

                switch (c)
                {
                        case 'k': // scroll up
                        case KEY_UP:
                                if (selline>0)
                                {
                                        selline--;
                                        redraw = 1;
                                }
                                check_curs_pos(&selline, taskcount);
                                break;
                        case 'j': // scroll down
                        case KEY_DOWN:
                                if (selline<taskcount-1)
                                {
                                        selline++;
                                        redraw = 1;
                                }
                                check_curs_pos(&selline, taskcount);
                                break;
                        case KEY_HOME: // go to first entry
                                selline = 0;
                                redraw = 1;
                                break;
                        case KEY_END: // go to last entry
                                selline = taskcount-1;
                                redraw = 1;
                                break;
                        case 'e': // edit task
                                def_prog_mode();
                                endwin();
                                /* edit_task(head, selline); */
                                task_action(head, selline, ACTION_EDIT);
                                reload_tasks(&head);
                                refresh();
                                wipe_screen(1, size);
                                redraw = 1;
                                break;
                        case 'r': // reload task list
                                reload_tasks(&head);
                                wipe_screen(1, size);
                                taskcount = task_count(head);
                                check_curs_pos(&selline, taskcount);
                                redraw = 1;
                                break;
                        case 'u': // undo
                                def_prog_mode();
                                endwin();
                                system("task undo");
                                refresh();
                                wipe_screen(1, size);
                                reload_tasks(&head);
                                taskcount = task_count(head);
                                check_curs_pos(&selline, taskcount);
                                redraw = 1;
                                break;
                        case 'd': // delete
                                def_prog_mode();
                                endwin();
                                task_action(head, selline, ACTION_DELETE);
                                refresh();
                                reload_tasks(&head);
                                taskcount = task_count(head);
                                check_curs_pos(&selline, taskcount);
                                wipe_screen(1, size);
                                redraw = 1;
                                break;
                        case 'c': // complete
                                def_prog_mode();
                                endwin();
                                task_action(head, selline, ACTION_COMPLETE);
                                refresh();
                                reload_tasks(&head);
                                taskcount = task_count(head);
                                check_curs_pos(&selline, taskcount);
                                wipe_screen(1, size);
                                redraw = 1;
                                break;
                        case 'a': // add new
                        case 'n':
                                def_prog_mode();
                                endwin();
                                task_add(taskcount);
                                refresh();
                                reload_tasks(&head);
                                taskcount = task_count(head);
                                check_curs_pos(&selline, taskcount);
                                wipe_screen(1, size);
                                redraw = 1;
                                break;
                        case 'q': // quit
                                done = 1;
                                break;
                        default: // unhandled
                                break;
                }
                if (done==1)
                        break;
                if (redraw==1)
                {
                        projlen = max_project_length(head);
                        desclen = size[0]-projlen-1-datelen;
                        print_title(size[0]);
                        print_task_list(head, selline, projlen, desclen, datelen);
                        refresh();
                }
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
        /* terminate ncurses */
        endwin();
        
        switch (sig)
        {
                case SIGINT:
                        puts("aborted");
                        break;
                case SIGSEGV:
                        puts("SEGFAULT");
                        break;
                default:
                        puts("done");
                        break;
        }

        /* free all structs here */
        exit(0);
}
/* }}} */

/* print_task_list {{{ */
void
print_task_list(const task *head, const short selected, const short projlen, const short desclen, const short datelen)
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
                if (cur->project==NULL)
                        bufstr = pad_string(" ", projlen, 0, 1, 'r');
                else
                        bufstr = pad_string(cur->project, projlen, 0, 1, 'r');
                mvaddstr(counter+1, 0, bufstr);
                free(bufstr);
                attrset(COLOR_PAIR(3+3*sel));
                bufstr = pad_string(cur->description, desclen, 0, 1, 'l');
                mvaddstr(counter+1, projlen+1, bufstr);
                free(bufstr);
                attrset(COLOR_PAIR(4+3*sel));
                if (cur->due!=(unsigned int)NULL)
                {
                        char *tmp;
                        tmp = utc_date(cur->due);
                        bufstr = pad_string(tmp, datelen, 0, 0, 'r');
                        free(tmp);
                }
                else if (cur->priority)
                {
                        char *tmp;
                        tmp = malloc(2*sizeof(char));
                        sprintf(tmp, "%c", cur->priority);
                        bufstr = pad_string(tmp, datelen, 0, 0, 'r');
                        free(tmp);
                }
                else
                        bufstr = pad_string(" ", datelen, 0, 0, 'r');
                mvaddstr(counter+1, projlen+desclen+1, bufstr);
                free(bufstr);
                counter++;
                cur = cur->next;
        }
}
/* }}} */

/* max_project_length {{{ */
char 
max_project_length(const task *head)
{
        char len = 0;
        task *cur;

        cur = head;
        while (cur!=NULL)
        {
                if (cur->project!=NULL)
                {
                        char l = strlen(cur->project);
                        if (l>len)
                                len = l;
                }
                cur = cur->next;
        }

        return len+1;
}
/* }}} */

/* task_count {{{ */
char
task_count(const task *head)
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
pad_string(char *str, int length, const int lpad, int rpad, const char align)
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

/* task_action {{{ */
void
task_action(task *head, const short pos, const char action)
{
        /* spawn a command to complete a task */
        task *cur;
        short p;
        char *cmd;
        char *actionstr;
        
        /* move to correct task */
        cur = head;
        for (p=0; p<pos; p++)
                cur = cur->next;

        /* determine action */
        actionstr = malloc(5*sizeof(char));
        switch(action)
        {
                case ACTION_EDIT:
                        strncpy(actionstr, "edit", 5);
                        break;
                case ACTION_COMPLETE:
                        strncpy(actionstr, "done", 5);
                        break;
                case ACTION_DELETE:
                        strncpy(actionstr, "del", 4);
                        break;
                default:
                        strncpy(actionstr, "view", 5);
                        break;
        }

        /* generate and run command */
        cmd = malloc(32*sizeof(char));
        sprintf(cmd, "task %s %d", actionstr, cur->index);
        free(actionstr);
        puts(cmd);
        system(cmd);
        free(cmd);
}
/* }}} */

/* task_add {{{ */
void
task_add(const char taskcount)
{
        /* create a new task by adding a generic task
         * then letting the user edit it
         */
        char *cmd;

        /* add new task */
        puts("task add new task");
        system("task add new task");

        /* edit task */
        cmd = malloc(32*sizeof(char));
        sprintf(cmd, "task edit %d", taskcount+1);
        puts(cmd);
        system(cmd);
        free(cmd);
}
/* }}} */

/* wipe_screen {{{ */
void
wipe_screen(const short start, const int size[2])
{
        /* clear the screen */
        int pos;
        char *blank;
        
        attrset(COLOR_PAIR(0));
        blank = pad_string(" ", size[0], 0, 0, 'r');

        for (pos=start; pos<size[1]; pos++)
        {
                mvaddstr(pos, 0, blank);
        }
        free(blank);
}
/* }}} */

/* reload_tasks {{{ */
void
reload_tasks(task **headptr)
{
        /* reset head with a new list of tasks */
        task *cur;

        logmsg("reloading tasks", 1);

        cur = *headptr;
        free_tasks(cur);

        (*headptr) = get_tasks();

        /* debug */
        cur = *headptr;
        while (cur!=NULL)
        {
                char *buffer;
                buffer = malloc(16*1024*sizeof(char));
                sprintf(buffer, "%d,%s,%s,%d,%d,%d,%d,%s,%c,%s", cur->index, cur->uuid, cur->tags, cur->start, cur->end, cur->entry, cur->due, cur->project, cur->priority, cur->description);
                logmsg(buffer, 2);
                free(buffer);
                cur = cur->next;
        }
}
/* }}} */

/* check_curs_pos {{{ */
void
check_curs_pos(short *pos, const char tasks) 
{
        /* check if the cursor is in a valid position */
        if ((*pos)<0)
                *pos = 0;
        if ((*pos)>=tasks)
                *pos = tasks-1;
}
/* }}} */

/* swap_tasks {{{ */
void
swap_tasks(task *a, task *b)
{
        /* swap the contents of two tasks */
        unsigned short ustmp;
        unsigned int uitmp;
        char *strtmp;
        char ctmp;

        ustmp = a->index;
        a->index = b->index;
        b->index = ustmp;

        strtmp = a->uuid;
        a->uuid = b->uuid;
        b->uuid = strtmp;

        strtmp = a->tags;
        a->tags = b->tags;
        b->tags = strtmp;

        uitmp = a->start;
        a->start = b->start;
        b->start = uitmp;

        uitmp = a->end;
        a->end = b->end;
        b->end = uitmp;

        uitmp = a->entry;
        a->entry = b->entry;
        b->entry = uitmp;

        uitmp = a->due;
        a->due = b->due;
        b->due = uitmp;

        strtmp = a->project;
        a->project = b->project;
        b->project = strtmp;

        ctmp = a->priority;
        a->priority = b->priority;
        b->priority = ctmp;

        strtmp = a->description;
        a->description = b->description;
        b->description = strtmp;
}
/* }}} */

/* log {{{ */
void
logmsg(const char *msg, const char minloglvl)
{
        /* log a message to a file */
        FILE *fp;

        /* determine if msg should be logged */
        if (minloglvl>loglvl)
                return;

        /* write log */
        fp = fopen(LOGFILE, "a");
        fprintf(fp, "%s\n", msg);
        fclose(fp);
}
/* }}} */

/* strip_quotes {{{ */
char *
strip_quotes(char *base)
{
        /* remove the first and last character from a string (quotes) */
        int len;

        /* remove first char */
        base++;
        len = strlen(base);

        /* remove last char - TODO: clean this up */
        if (base[len-1] != '\'')
                base[len-2] = '\0';
        else
                base[len-1] = '\0';

        return base;
}
/* }}} */

/* print_title {{{ */
void
print_title(const int width)
{
        /* print the title of the window */
        attrset(COLOR_PAIR(1));
        char *title = pad_string("task ncurses - by mjheagle", width, 0, 0, 'l');
        mvaddstr(0, 0, title);
        free(title);
}
/* }}} */

/* sort_wrapper {{{ */
void
sort_wrapper(task *first)
{
        /* a wrapper around sort_tasks that finds the last element
         * to pass to that function
         */
        task *last;

        /* loop through looking for last item */
        last = first;
        while (last->next != NULL)
                last = last->next;

        /* run sort with last value */
        sort_tasks(first, last);
}
/* }}} */

/* sort_tasks {{{ */
void
sort_tasks(task *first, task *last)
{
        /* sort the list of tasks */
        task *start, *cur, *oldcur;

        /* check if we are done */
        if (first==last)
                return;

        /* set start and current */
        start = first;
        cur = start->next;

        /* iterate through to right end, sorting as we go */
        while (1)
        {
                if (compare_tasks(start, cur)==1)
                        swap_tasks(start, cur);
                if (cur==last)
                        break;
                cur = cur->next;
        }

        /* swap first and cur */
        swap_tasks(first, cur);

        /* save this cur */
        oldcur = cur;

        /* sort left side */
        cur = cur->prev;
        if (cur != NULL)
        {
                if ((first->prev != cur) && (cur->next != first))
                        sort_tasks(first, cur);
        }

        /* sort right side */
        cur = oldcur->next;
	if (cur != NULL)
	{
		if ((cur->prev != last) && (last->next != cur))
                        sort_tasks(cur, last);
	}
}
/* }}} */

/* compare_tasks {{{ */
char
compare_tasks(const task *a, const task *b)
{
        /* compare two tasks to determine order */
        char ret = 0;
        int tmp;

        /* TODO: improve comparison methods */
        tmp = strcmp(a->project, b->project);
        if (tmp<0)
                ret = 1;

        return ret;
}
/* }}} */
