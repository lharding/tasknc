/*
 * task nc - a ncurses wrapper around taskwarrior
 * by mjheagle
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include <time.h>
#include <regex.h>
#include "config.h"

/* macros {{{ */
/* wiping functions */
#define wipe_tasklist() wipe_screen(1, size[1]-2)
#define wipe_statusbar() wipe_screen(size[1]-1, size[1]-1)

/* program information */
#define NAME "taskwarrior ncurses shell"
#define SHORTNAME "tasknc"
#define VERSION "0.4"
#define AUTHOR "mjheagle"

/* field lengths */
#define UUIDLENGTH 38
#define DATELENGTH 10

/* action definitions */
#define ACTION_EDIT 0
#define ACTION_COMPLETE 1
#define ACTION_DELETE 2
#define ACTION_VIEW 3

/* ncurses settings */
#define NCURSES_WAIT 500
#define NCURSES_MODE_STD 0
#define NCURSES_MODE_STD_BLOCKING 1
#define NCURSES_MODE_STRING 2

/* filter modes */
#define FILTER_BY_STRING 0
#define FILTER_CLEAR 1
#define FILTER_DESCRIPTION 2
#define FILTER_TAGS 3
#define FILTER_PROJECT 4

/* regex options */
#define REGEX_OPTS REG_ICASE|REG_EXTENDED|REG_NOSUB|REG_NEWLINE
/* }}} */

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
        char is_filtered;
        struct _task *prev;
        struct _task *next;
} task;
/* }}} */

/* function prototypes {{{ */
static void check_curs_pos(void);
static void check_screen_size(short);
static char compare_tasks(const task *, const task *, const char);
static void filter_tasks(task *, const char, const char *, const char *);
static void find_next_search_result(task *, task *);
static char free_task(task *);
static void free_tasks(task *);
static unsigned short get_task_id(char *);
static task *get_tasks(void);
static void help(void);
static void logmsg(const char *, const char);
static task *malloc_task(void);
static char match_string(const char *, const char *);
static char max_project_length(task *);
static task *sel_task(task *);
static void nc_colors(void);
static void nc_end(int);
static void nc_main(task *);
static char *pad_string(char *, int, const int, int, const char);
static task *parse_task(char *);
static void print_task_list(task *, const short, const short, const short);
static void print_title(const int);
static void print_version(void);
static void reload_tasks(task **);
static void set_curses_mode(char);
static void sort_tasks(task *, task *);
static void sort_wrapper(task *);
static char *strip_quotes(char *);
static void statusbar_message(const char *, const int);
static void swap_tasks(task *, task *);
static int task_action(task *, const char);
static void task_add(void);
static void task_count(task *);
static char task_match(const task *, const char *);
static char *utc_date(const unsigned int);
static void wipe_screen(const short, const short);
/* }}} */

/* global variables {{{ */
char loglvl = 0;                /* used by logmsg to determine whether msgs should be written to logfile */
short pageoffset = 0;           /* number of tasks page is offset */
time_t sb_timeout = 0;          /* when statusbar should be cleared */
char *searchstring = NULL;      /* currently active search string */
short selline = 0;              /* selected line number */
int size[2];                    /* size of the ncurses window */
char sortmode = 'd';            /* used by compare_tasks to determine what algorithm to use to compare tasks */
char taskcount;                 /* number of tasks */
char totaltaskcount;            /* number of tasks with no filters applied */
/* }}} */

void check_curs_pos(void) /* {{{ */
{
        /* check if the cursor is in a valid position */
        const short onscreentasks = size[1]-3;
        char *tmpstr;

        /* check for a valid selected line number */
        if (selline<0)
                selline = 0;
        else if (selline>=taskcount)
                selline = taskcount-1;

        /* check if page offset needs to be changed */
        if (selline<pageoffset)
                pageoffset = selline;
        else if (selline>pageoffset+onscreentasks)
                pageoffset = selline - onscreentasks;

        /* log cursor position */
        tmpstr = malloc(128*sizeof(char));
        sprintf(tmpstr, "selline:%d offset:%d taskcount:%d perscreen:%d", selline, pageoffset, taskcount, size[1]-3);
        logmsg(tmpstr, 3);
        free(tmpstr);
} /* }}} */

void check_screen_size(short projlen) /* {{{ */
{
        /* check for a screen thats too small */
        int count = 0;

        do 
        {
                if (count)
                {
                        if (count==1)
                        {
                                wipe_statusbar();
                                wipe_tasklist();
                        }
                        attrset(COLOR_PAIR(8));
                        mvaddstr(0, 0, "screen dimensions too small");
                        refresh();
                        attrset(COLOR_PAIR(0));
                        usleep(100000);
                }
                count++;
                getmaxyx(stdscr, size[1], size[0]);
        } while (size[0]<DATELENGTH+20+projlen || size[1]<5);
} /* }}} */

char compare_tasks(const task *a, const task *b, const char sort_mode) /* {{{ */
{
        /* compare two tasks to determine order
         * a return of 1 means that the tasks should be swapped (b comes before a)
         */
        char ret = 0;
        int tmp;

        /* determine sort algorithm and apply it */
        switch (sort_mode)
        {
                case 'n':       // sort by index
                        if (a->index<b->index)
                                ret = 1;
                        break;
                default:
                case 'p':       // sort by project name => index
                        if (a->project == NULL)
                        {
                                if (b->project != NULL)
                                        ret = 1;
                                break;
                        }
                        if (b->project == NULL)
                                break;
                        tmp = strcmp(a->project, b->project);
                        if (tmp<0)
                                ret = 1;
                        if (tmp==0)
                                ret = compare_tasks(a, b, 'n');
                        break;
                case 'd':       // sort by due date => priority => project => index
                        if (a->due == 0)
                        {
                                if (b->due == 0)
                                        ret = compare_tasks(a, b, 'r');
                                break;
                        }
                        if (b->due == 0)
                        {
                                ret = 1;
                                break;
                        }
                        if (a->due<b->due)
                                ret = 1;
                        break;
                case 'r':       // sort by priority => project => index
                        if (a->priority == 0)
                        {
                                if (b->priority == 0)
                                        ret = compare_tasks(a, b, 'p');
                                break;
                        }
                        if (b->priority == 0)
                        {
                                ret = 1;
                                break;
                        }
                        if (a->priority == b->priority)
                        {
                                ret = compare_tasks(a, b, 'p');
                                break;
                        }
                        switch (b->priority)
                        {
                                case 'H':
                                default:
                                        break;
                                case 'M':
                                        if (a->priority=='H')
                                                ret = 1;
                                        break;
                                case 'L':
                                        if (a->priority=='M' || a->priority=='H')
                                                ret = 1;
                                        break;
                        }
                        break;
        }

        return ret;
} /* }}} */

void filter_tasks(task *head, const char filter_mode, const char *filter_comparison, const char *filter_value) /* {{{ */
{
        /* iterate through task list and filter them */
        task *cur = head;

        /* reset task counters */
        taskcount = 0;
        totaltaskcount = 0;

        /* loop through tasks */
        while (cur!=NULL)
        {
                switch (filter_mode)
                {
                        case FILTER_DESCRIPTION:
                                cur->is_filtered = match_string(cur->description, filter_value);
                                break;
                        case FILTER_TAGS:
                                cur->is_filtered = match_string(cur->tags, filter_value);
                                break;
                        case FILTER_PROJECT:
                                cur->is_filtered = match_string(cur->project, filter_value);
                                break;
                        case FILTER_CLEAR:
                                cur->is_filtered = 1;
                                break;
                        case FILTER_BY_STRING:
                        default:
                                cur->is_filtered = task_match(cur, filter_value);
                                break;
                }
                taskcount += cur->is_filtered;
                totaltaskcount++;
                cur = cur->next;
        }
} /* }}} */

void find_next_search_result(task *head, task *pos) /* {{{ */
{
        /* find the next search result in the list of tasks */
        task *cur;
        char *tmp;

        cur = pos;
        while (1)
        {
                /* move to next item */
                cur = cur->next;

                /* move to head if end of list is reached */
                if (cur == NULL)
                {
                        cur = head;
                        if (cur->is_filtered)
                                selline = 0;
                        else
                                selline = -1;
                        logmsg("search wrapped", 3);
                }

                /* skip tasks that are filtered out */
                else if (cur->is_filtered)
                        selline++;
                else
                        continue;

                /* check for match */
                if (task_match(cur, searchstring)==1)
                        return;

                /* stop if full loop was made */
                if (cur==pos)
                        break;
        }

        tmp = malloc((13+strlen(searchstring))*sizeof(char));
        sprintf(tmp, "no matches: %s", searchstring);
        statusbar_message(tmp, 3);
        free(tmp);

        return;
} /* }}} */

char free_task(task *tsk) /* {{{ */
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
} /* }}} */

void free_tasks(task *head) /* {{{ */
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
} /* }}} */

unsigned short get_task_id(char *uuid) /* {{{ */
{
        /* given a task uuid, find its id
         * we do this using a custom report
         * necessary to do without uuid addressing in task v2
         */
        FILE *cmd;
        char line[128], format[128];
        int ret;
        unsigned short id = 0;

        /* generate format to scan for */
        sprintf(format, "%s %%hu", uuid);

        /* run command */
        cmd = popen("task rc.report.all.columns:uuid,id rc.report.all.labels:UUID,id rc.report.all.sort:id- all status:pending rc._forcecolor=no", "r");
        while (fgets(line, sizeof(line)-1, cmd) != NULL)
        {
                ret = sscanf(line, format, &id);
                if (ret>0)
                        break;
        }
        pclose(cmd);

        return id;
} /* }}} */

task *get_tasks(void) /* {{{ */
{
        FILE *cmd;
        char line[TOTALLENGTH];
        unsigned short counter = 0;
        task *head, *last;

        /* run command */
        cmd = popen("task export.csv status:pending", "r");

        /* parse output */
        last = NULL;
        head = NULL;
        while (fgets(line, sizeof(line)-1, cmd) != NULL)
        {
                task *this;
                if (counter==0)
                        this = NULL;
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

        /* sort tasks */
        if (head!=NULL)
                sort_wrapper(head);

        return head;
} /* }}} */

void help(void) /* {{{ */
{
        /* print a list of options and program info */
        print_version();
        puts("\noptions:");
        puts("  -l [value]: set log level");
        puts("  -d: debug mode (no ncurses run)");
        puts("  -h: print this help message");
        puts("  -v: print the version of tasknc");
} /* }}} */

void logmsg(const char *msg, const char minloglvl) /* {{{ */
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
} /* }}} */

task *malloc_task(void) /* {{{ */
{
        /* allocate memory for a new task 
         * and initialize values where ncessary 
         */
        task *tsk = malloc(sizeof(task));
        if (tsk)
                memset(tsk, 0, sizeof(task));
        else
                return NULL;

        tsk->index = 0;
        tsk->uuid = NULL;
        tsk->tags = NULL;
        tsk->start = 0;
        tsk->end = 0;
        tsk->entry = 0;
        tsk->due = 0;
        tsk->project = NULL;
        tsk->priority = 0;
        tsk->description = NULL;
        tsk->is_filtered = 1;
        tsk->next = NULL;
        tsk->prev = NULL;

        return tsk;
} /* }}} */

char match_string(const char *haystack, const char *needle) /* {{{ */
{
        /* match a string to a regex */
        regex_t regex;
        char ret;

        /* compile and run regex */
        if (regcomp(&regex, needle, REGEX_OPTS) != 0)
                return 0;
        if (regexec(&regex, haystack, 0, 0, 0) != REG_NOMATCH)
                ret = 1;
        else
                ret = 0;
        regfree(&regex);
        return ret;
} /* }}} */

char max_project_length(task *head) /* {{{ */
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
} /* }}} */

void nc_colors(void) /* {{{ */
{
        if (has_colors())
        {
                start_color();
                use_default_colors();
                init_pair(1, COLOR_BLUE,        COLOR_BLACK);   /* title bar */
                init_pair(2, COLOR_GREEN,       -1);            /* project */
                init_pair(3, COLOR_CYAN,        -1);            /* description */
                init_pair(4, COLOR_YELLOW,      -1);            /* date */
                init_pair(5, COLOR_BLACK,       COLOR_GREEN);   /* selected project */
                init_pair(6, COLOR_BLACK,       COLOR_CYAN);    /* selected description */
                init_pair(7, COLOR_BLACK,       COLOR_YELLOW);  /* selected date */
                init_pair(8, COLOR_RED,         -1);            /* error message */
        }
} /* }}} */

void nc_end(int sig) /* {{{ */
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
} /* }}} */

void nc_main(task *head) /* {{{ */
{
        /* ncurses main function */
        WINDOW *stdscr;
        char *tmpstr;
        int c, tmp, oldsize[2], ret;
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
        
        /* set curses settings */
        set_curses_mode(NCURSES_MODE_STD);

        /* print main screen */
        check_screen_size(projlen);
        getmaxyx(stdscr, oldsize[1], oldsize[0]);
        desclen = oldsize[0]-projlen-1-datelen;
        task_count(head);
        print_title(oldsize[0]);
        attrset(COLOR_PAIR(0));
        print_task_list(head, projlen, desclen, datelen);
        refresh();

        /* main loop */
        while (1)
        {
                /* set variables for determining actions */
                char done = 0;
                char redraw = 0;
                char reload = 0;

                /* get the screen size */
                getmaxyx(stdscr, size[1], size[0]);

                /* check for a screen thats too small */
                check_screen_size(projlen);

                /* check for size changes */
                if (size[0]!=oldsize[0] || size[1]!=oldsize[1])
                {
                        redraw = 1;
                        wipe_statusbar();
                }
                for (tmp=0; tmp<2; tmp++)
                        oldsize[tmp] = size[tmp];

                /* get a character */
                c = getch();

                /* handle the character */
                switch (c)
                {
                        case 'k': // scroll up
                        case KEY_UP:
                                if (selline>0)
                                {
                                        selline--;
                                        redraw = 1;
                                }
                                check_curs_pos();
                                break;
                        case 'j': // scroll down
                        case KEY_DOWN:
                                if (selline<taskcount-1)
                                {
                                        selline++;
                                        redraw = 1;
                                }
                                check_curs_pos();
                                break;
                        case KEY_HOME: // go to first entry
                                selline = 0;
                                redraw = 1;
                                check_curs_pos();
                                break;
                        case KEY_END: // go to last entry
                                selline = taskcount-1;
                                redraw = 1;
                                check_curs_pos();
                                break;
                        case 'e': // edit task
                                def_prog_mode();
                                endwin();
                                ret = task_action(head, ACTION_EDIT);
                                reload = 1;
                                if (ret==0)
                                        statusbar_message("task edited", 3);
                                else
                                        statusbar_message("task editing failed", 3);
                                break;
                        case 'r': // reload task list
                                reload = 1;
                                statusbar_message("task list reloaded", 3);
                                break;
                        case 'u': // undo
                                def_prog_mode();
                                endwin();
                                ret = system("task undo");
                                refresh();
                                reload = 1;
                                if (ret==0)
                                        statusbar_message("undo executed", 3);
                                else
                                        statusbar_message("undo execution failed", 3);
                                break;
                        case 'd': // delete
                                def_prog_mode();
                                endwin();
                                ret = task_action(head, ACTION_DELETE);
                                refresh();
                                reload = 1;
                                if (ret==0)
                                        statusbar_message("task deleted", 3);
                                else
                                        statusbar_message("task delete failed", 3);
                                break;
                        case 'c': // complete
                                def_prog_mode();
                                endwin();
                                ret = task_action(head, ACTION_COMPLETE);
                                refresh();
                                reload = 1;
                                wipe_tasklist();
                                if (ret==0)
                                        statusbar_message("task completed", 3);
                                else
                                        statusbar_message("task complete failed", 3);
                                break;
                        case 'a': // add new
                                def_prog_mode();
                                endwin();
                                task_add();
                                refresh();
                                reload = 1;
                                statusbar_message("task added", 3);
                                break;
                        case 'v': // view info
                        case KEY_ENTER:
                        case 13:
                                def_prog_mode();
                                endwin();
                                task_action(head, ACTION_VIEW);
                                refresh();
                                break;
                        case 's': // re-sort list
                                attrset(COLOR_PAIR(0));
                                statusbar_message("enter sort mode: iNdex, Project, Due, pRiority", 1);
                                set_curses_mode(NCURSES_MODE_STD_BLOCKING);
                                c = getch();
                                set_curses_mode(NCURSES_MODE_STD);
                                switch (c)
                                {
                                        case 'n':
                                        case 'p':
                                        case 'd':
                                        case 'r':
                                                sortmode = c;
                                                sort_wrapper(head);
                                                break;
                                        case 'N':
                                        case 'P':
                                        case 'D':
                                        case 'R':
                                                sortmode = c+32;
                                                sort_wrapper(head);
                                                break;
                                        default:
                                                statusbar_message("invalid sort mode", 3);
                                                break;
                                }
                                redraw = 1;
                                break;
                        case '/': // search
                                statusbar_message("search phrase: ", -1);
                                set_curses_mode(NCURSES_MODE_STRING);
                                /* store search string  */
                                if (searchstring!=NULL)
                                        free(searchstring);
                                searchstring = malloc((size[0]-16)*sizeof(char));
                                getstr(searchstring);
                                sb_timeout = time(NULL) + 3;
                                set_curses_mode(NCURSES_MODE_STD);
                                /* go to first result */
                                find_next_search_result(head, sel_task(head));
                                check_curs_pos();
                                redraw = 1;
                                break;
                        case 'n': // next search result
                                if (searchstring!=NULL)
                                {
                                        find_next_search_result(head, sel_task(head));
                                        check_curs_pos();
                                        redraw = 1;
                                }
                                else
                                        statusbar_message("no active search string", 3);
                                break;
                        case 'f': // filter
                                statusbar_message("filter by: Any Clear Proj Desc Tag", 3);
                                set_curses_mode(NCURSES_MODE_STD_BLOCKING);
                                c = getch();
                                wipe_statusbar();
                                if (strchr("acdptACDPT", c)==NULL)
                                {
                                        statusbar_message("invalid filter mode", 3);
                                        break;
                                }
                                if (strchr("cC", c)==NULL)
                                {
                                        statusbar_message("filter string: ", 1);
                                        set_curses_mode(NCURSES_MODE_STRING);
                                        tmpstr = malloc((size[0]-16)*sizeof(char));
                                        getstr(tmpstr);
                                }
                                set_curses_mode(NCURSES_MODE_STD);
                                switch (c)
                                {
                                        case 'a':
                                        case 'A':
                                                filter_tasks(head, FILTER_BY_STRING, NULL, tmpstr);
                                                free(tmpstr);
                                                break;
                                        case 'c':
                                        case 'C':
                                                filter_tasks(head, FILTER_CLEAR, NULL, NULL);
                                                break;
                                        case 'd':
                                        case 'D':
                                                filter_tasks(head, FILTER_DESCRIPTION, NULL, tmpstr);
                                                break;
                                        case 'p':
                                        case 'P':
                                                filter_tasks(head, FILTER_PROJECT, NULL, tmpstr);
                                                break;
                                        case 't':
                                        case 'T':
                                                filter_tasks(head, FILTER_TAGS, NULL, tmpstr);
                                                break;
                                        default:
                                                statusbar_message("invalid filter mode", 3);
                                                break;
                                }
                                /* check if task list is empty after filtering */
                                if (taskcount==0)
                                {
                                        filter_tasks(head, FILTER_CLEAR, NULL, NULL);
                                        statusbar_message("filter yielded no results; reset", 3);
                                }
                                else
                                        statusbar_message("filter applied", 3);
                                check_curs_pos();
                                redraw = 1;
                                break;
                        case 'y': // sync
                                def_prog_mode();
                                endwin();
                                ret = system("yes n | task merge");
                                if (ret==0)
                                        ret = system("task push");
                                refresh();
                                if (ret==0)
                                        statusbar_message("tasks synchronized", 3);
                                else
                                        statusbar_message("task syncronization failed", 3);
                                break;
                        case 'q': // quit
                                done = 1;
                                break;
                        case ERR: // no key was pressed
                                break;
                        default: // unhandled
                                tmpstr = malloc(20*sizeof(char));
                                sprintf(tmpstr, "unhandled key: %c", c);
                                attrset(COLOR_PAIR(0));
                                statusbar_message(tmpstr, 3);
                                free(tmpstr);
                                break;
                }
                if (done==1)
                        break;
                if (reload==1)
                {
                        reload_tasks(&head);
                        task_count(head);
                        check_curs_pos();
                        print_title(size[0]);
                        redraw = 1;
                }
                if (redraw==1)
                {
                        wipe_tasklist();
                        projlen = max_project_length(head);
                        desclen = size[0]-projlen-1-datelen;
                        print_title(size[0]);
                        print_task_list(head, projlen, desclen, datelen);
                        refresh();
                }
                if (sb_timeout>0 && sb_timeout<time(NULL))
                {
                        sb_timeout = 0;
                        wipe_statusbar();
                }
        }

        free_tasks(head);

        delwin(stdscr);
} /* }}} */

char *pad_string(char *argstr, int length, const int lpad, int rpad, const char align) /* {{{ */
{
        /* function to add padding to strings and align them with spaces */
        char *ft;
        char *ret;
        char *str;

        /* copy argstr to placeholder that we can modify */
        str = malloc((strlen(argstr)+1)*sizeof(char));
        str = strcpy(str, argstr);

        /* cut string if necessary */
        if ((int)strlen(str)>length-lpad-rpad)
        {
                str[length-lpad-rpad] = '\0';
                int i;
                for (i=1; i<=3; i++)
                        str[length-lpad-rpad-i] = '.';
        }

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
        free(str);

        return ret;
} /* }}} */

task *parse_task(char *line) /* {{{ */
{
        task *tsk = malloc_task();
        char *token, *lastchar, *tmpstr;
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
                                if (tsk->uuid==NULL)
                                {
                                        tsk->uuid = malloc(UUIDLENGTH*sizeof(char));
                                        token = strip_quotes(token);
                                        sprintf(tsk->uuid, "%s", token);
                                }
                                break;
                        case 1: // status
                                break;
                        case 2: // tags
                                if (token==strrchr(token, '\'')) 
                                {
                                        tsk->tags = malloc(TAGSLENGTH*sizeof(char));
                                        strcpy(tsk->tags, ++token);
                                        do
                                        {
                                                token = strtok(NULL, ",");
                                                strcat(tsk->tags, ",");
                                                strcat(tsk->tags, token);
                                        } while (strrchr(token, '\'')==NULL);
                                        tmpstr = strrchr(tsk->tags, '\'');
                                        (*tmpstr) = '\0';
                                }
                                else if (tsk->tags==NULL)
                                {
                                        tsk->tags = malloc(TAGSLENGTH*sizeof(char));
                                        strcpy(tsk->tags, strip_quotes(token));
                                }
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
                                if (token==strrchr(token, '\'')) 
                                {
                                        tsk->project = malloc(PROJECTLENGTH*sizeof(char));
                                        strcpy(tsk->project, ++token);
                                        do
                                        {
                                                token = strtok(NULL, ",");
                                                strcat(tsk->project, ",");
                                                strcat(tsk->project, token);
                                        } while (strrchr(token, '\'')==NULL);
                                        tmpstr = strrchr(tsk->project, '\'');
                                        (*tmpstr) = '\0';
                                }
                                else if (tsk->project==NULL)
                                {
                                        tsk->project = malloc(PROJECTLENGTH*sizeof(char));
                                        strcpy(tsk->project, strip_quotes(token));
                                }
                                break;
                        case 9: // priority
                                tmp = sscanf(token, "'%c'", &tsk->priority);
                                break;
                        case 10: // fg
                                break;
                        case 11: // bg
                                break;
                        case 12: // description
                                if (token==strrchr(token, '\'')) 
                                {
                                        tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
                                        strcpy(tsk->description, ++token);
                                        do
                                        {
                                                token = strtok(NULL, ",");
                                                strcat(tsk->description, ",");
                                                strcat(tsk->description, token);
                                        } while (strrchr(token, '\'')==NULL);
                                        tmpstr = strrchr(tsk->description, '\'');
                                        (*tmpstr) = '\0';
                                }
                                else if (tsk->description==NULL)
                                {
                                        tsk->description = malloc(DESCRIPTIONLENGTH*sizeof(char));
                                        strcpy(tsk->description, strip_quotes(token));
                                }
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
} /* }}} */

void print_task_list(task *head, const short projlen, const short desclen, const short datelen) /* {{{ */
{
        task *cur;
        short counter = 0;
        char *bufstr;
        const short onscreentasks = size[1]-3;
        short thisline = 0;

        cur = head;
        while (cur!=NULL)
        {
                char skip = 0;
                char sel = 0;

                /* skip filtered tasks */
                if (!cur->is_filtered)
                        skip = 1;

                /* skip tasks that are off screen */
                else if (counter<pageoffset)
                        skip = 1;
                else if (counter>pageoffset+onscreentasks)
                        skip = 1;

                /* skip row if necessary */
                if (skip==1)
                {
                        if (cur->is_filtered)
                                counter++;
                        cur = cur->next;
                        continue;
                }

                /* check if item is selected */
                if (counter==selline)
                        sel = 1;
                
                /* move to next line */
                thisline++;

                /* print project */
                attrset(COLOR_PAIR(2+3*sel));
                if (cur->project==NULL)
                        bufstr = pad_string(" ", projlen, 0, 1, 'r');
                else
                        bufstr = pad_string(cur->project, projlen, 0, 1, 'r');
                mvaddstr(thisline, 0, bufstr);
                free(bufstr);
                
                /* print description */
                attrset(COLOR_PAIR(3+3*sel));
                bufstr = pad_string(cur->description, desclen, 0, 1, 'l');
                mvaddstr(thisline, projlen+1, bufstr);
                free(bufstr);

                /* print due date or priority if available */
                attrset(COLOR_PAIR(4+3*sel));
                if (cur->due != 0)
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
                mvaddstr(thisline, projlen+desclen+1, bufstr);
                free(bufstr);

                /* move to next item */
                counter++;
                cur = cur->next;
        }
} /* }}} */

void print_title(const int width) /* {{{ */
{
        /* print the window title bar */
        char *tmp0, *tmp1;

        /* print program info */
        attrset(COLOR_PAIR(1));
        tmp0 = malloc(width*sizeof(char));
        sprintf(tmp0, "%s v%s  (%d/%d)", SHORTNAME, VERSION, taskcount, totaltaskcount);
        tmp1 = pad_string(tmp0, width, 0, 0, 'l');
        mvaddstr(0, 0, tmp1);
        free(tmp0);
        free(tmp1);

        /* print the current date */
        tmp0 = utc_date(0);
        tmp1 = pad_string(tmp0, DATELENGTH, 0, 0, 'r');
        mvaddstr(0, width-DATELENGTH, tmp1);
        free(tmp0);
        free(tmp1);
} /* }}} */

void print_version(void) /* {{{ */
{
        /* print info about the currently running program */
        printf("%s v%s by %s\n", NAME, VERSION, AUTHOR);
} /* }}} */

void reload_tasks(task **headptr) /* {{{ */
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
} /* }}} */

void set_curses_mode(char curses_mode) /* {{{ */
{
        /* set curses settings for various common modes */
        switch (curses_mode)
        {
                case NCURSES_MODE_STD:
                        keypad(stdscr, TRUE);   /* enable keyboard mapping */
                        nonl();                 /* tell curses not to do NL->CR/NL on output */
                        cbreak();               /* take input chars one at a time, no wait for \n */
                        noecho();               /* dont echo input */
                        nc_colors();            /* initialize colors */
                        curs_set(0);            /* set cursor invisible */
                        timeout(NCURSES_WAIT);  /* timeout getch */
                        break;
                case NCURSES_MODE_STD_BLOCKING:
                        keypad(stdscr, TRUE);   /* enable keyboard mapping */
                        nonl();                 /* tell curses not to do NL->CR/NL on output */
                        cbreak();               /* take input chars one at a time, no wait for \n */
                        noecho();               /* dont echo input */
                        nc_colors();            /* initialize colors */
                        curs_set(0);            /* set cursor invisible */
                        timeout(-1);            /* no timeout on getch */
                        break;
                case NCURSES_MODE_STRING:
                        curs_set(2);            /* set cursor visible */
                        nocbreak();             /* wait for \n */
                        echo();                 /* echo input */
                        timeout(-1);            /* no timeout on getch */
                        break;
                default:
                        break;
        }
} /* }}} */

task *sel_task(task *head) /* {{{ */
{
        /* navigate to the selected line
         * and return its task pointer
         */
        task *cur;
        short i = -1;

        cur = head;
        while (cur!=NULL)
        {
                i += cur->is_filtered;
                if (i==selline)
                        break;
                cur = cur->next;
        }

        return cur;
} /* }}} */

void sort_tasks(task *first, task *last) /* {{{ */
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
                if (compare_tasks(start, cur, sortmode)==1)
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
} /* }}} */

void sort_wrapper(task *first) /* {{{ */
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
} /* }}} */

void statusbar_message(const char *message, const int dtmout) /* {{{ */
{
        /* print a message in the statusbar */
        wipe_statusbar();

        mvaddstr(size[1]-1, 0, message);
        if (dtmout>=0)
                sb_timeout = time(NULL) + dtmout;
        refresh();
} /* }}} */

char *strip_quotes(char *base) /* {{{ */
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
} /* }}} */

void swap_tasks(task *a, task *b) /* {{{ */
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

        ctmp = a->is_filtered;
        a->is_filtered = b->is_filtered;
        b->is_filtered = ctmp;
} /* }}} */

int task_action(task *head, const char action) /* {{{ */
{
        /* spawn a command to perform an action on a task */
        task *cur;
        char *cmd, *actionstr, wait;
        int ret;
        
        /* move to correct task */
        cur = sel_task(head);

        /* determine action */
        actionstr = malloc(5*sizeof(char));
        wait = 0;
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
                case ACTION_VIEW:
                default:
                        strncpy(actionstr, "info", 5);
                        wait = 1;
                        break;
        }

        /* update task index if necessary */
        cur->index = get_task_id(cur->uuid);
        if (cur->index==0)
                return -1;

        /* generate and run command */
        cmd = malloc(32*sizeof(char));
        sprintf(cmd, "task %s %d", actionstr, cur->index);
        free(actionstr);
        puts(cmd);
        ret = system(cmd);
        free(cmd);
        if (wait)
        {
                puts("press ENTER to return");
                getchar();
        }
        return ret;
} /* }}} */

void task_add(void) /* {{{ */
{
        /* create a new task by adding a generic task
         * then letting the user edit it
         */
        FILE *cmdout;
        char *cmd, line[TOTALLENGTH];
        const char addstr[] = "Created task ";
        unsigned short tasknum;

        /* add new task */
        puts("task add new task");
        cmdout = popen("task add new task", "r");
        while (fgets(line, sizeof(line)-1, cmdout) != NULL)
        {
                if (strncmp(line, addstr, strlen(addstr))==0)
                        if (sscanf(line, "Created task %hu.", &tasknum))
                                break;
        }
        pclose(cmdout);

        /* edit task */
        cmd = malloc(32*sizeof(char));
        sprintf(cmd, "task edit %d", tasknum);
        puts(cmd);
        system(cmd);
        free(cmd);
} /* }}} */

void task_count(task *head) /* {{{ */
{
        taskcount = 0;
        totaltaskcount = 0;
        task *cur;

        cur = head;
        while (cur!=NULL)
        {
                taskcount++;
                totaltaskcount++;
                cur = cur->next;
        }
} /* }}} */

static char task_match(const task *cur, const char *str) /* {{{ */
{
        if (match_string(cur->project, str) ||
                        match_string(cur->description, str) ||
                        match_string(cur->tags, str))
                return 1;
        else
                return 0;
} /* }}} */

char *utc_date(const unsigned int timeint) /* {{{ */
{
        /* convert a utc time uint to a string */
        struct tm tmr, *now;
        time_t cur;
        char *timestr, *srcstr;

        /* convert the input timeint to a string */
        srcstr = malloc(16*sizeof(char));
        sprintf(srcstr, "%d", timeint);

        /* extract time struct from string */
        strptime(srcstr, "%s", &tmr);
        free(srcstr);

        /* get current time */
        time(&cur);
        now = localtime(&cur);

        /* set time to now if 0 was the argument */
        if (timeint==0)
                tmr = *now;

        /* convert thte time to a formatted string */
        timestr = malloc(TIMELENGTH*sizeof(char));
        if (now->tm_year != tmr.tm_year)
                strftime(timestr, TIMELENGTH, "%F", &tmr);
        else
                strftime(timestr, TIMELENGTH, "%b %d", &tmr);

        return timestr;
} /* }}} */

void wipe_screen(const short startl, const short stopl) /* {{{ */
{
        /* clear the screen except the title and status bars */
        int pos;
        char *blank;
        
        attrset(COLOR_PAIR(0));
        blank = pad_string(" ", size[0], 0, 0, 'r');

        for (pos=startl; pos<=stopl; pos++)
        {
                mvaddstr(pos, 0, blank);
        }
        free(blank);
} /* }}} */

int main(int argc, char **argv)
{
        /* declare variables */
        task *head;
        int c, debug = 0;

        /* handle arguments */
        while ((c = getopt(argc, argv, "l:hvd")) != -1)
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
                        case 'd':
                                debug = 1;
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
        if (head==NULL)
        {
                puts("it appears that your task list is empty");
                printf("please add some tasks for %s to manage\n", SHORTNAME);
                return 1;
        }

        /* run ncurses */
        if (!debug)
        {
                logmsg("running gui", 1);
                nc_main(head);
                nc_end(0);
        }

        /* done */
        if (searchstring!=NULL)
                free(searchstring);
        logmsg("exiting", 1);
        return 0;
}
