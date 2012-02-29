/*
 * tasknc config
 * by mjheagle
*/

#define NAME "taskwarrior ncurses shell"
#define SHORTNAME "tasknc"
#define VERSION "0.2"
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
#define ACTION_EDIT 0
#define ACTION_COMPLETE 1
#define ACTION_DELETE 2
#define ACTION_VIEW 3
#define NCURSES_WAIT 500
