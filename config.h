/*
 * tasknc config
 * by mjheagle
*/

#define NAME "taskwarrior ncurses shell"
#define SHORTNAME "tasknc"
#define VERSION "0.3"
#define AUTHOR "mjheagle"

/* user defined lengths */
#define MAXTASKS 1024
#define TOTALLENGTH 1024
#define TAGSLENGTH 128
#define PROJECTLENGTH 64
#define DESCRIPTIONLENGTH 512
#define TIMELENGTH 32
#define LOGFILE "runlog"

/* static */
#define UUIDLENGTH 64 /* only needs to be 38, added for debug */
#define DATELENGTH 10
#define ACTION_EDIT 0           /* action definitions */
#define ACTION_COMPLETE 1
#define ACTION_DELETE 2
#define ACTION_VIEW 3
#define NCURSES_WAIT 500        /* ncurses settings */
#define NCURSES_MODE_STD 0
#define NCURSES_MODE_STD_BLOCKING 1
#define NCURSES_MODE_STRING 2
#define FILTER_BY_STRING 0      /* filter modes */
#define FILTER_CLEAR 1 
#define FILTER_DESCRIPTION 2
#define FILTER_TAGS 3
#define FILTER_PROJECT 4
