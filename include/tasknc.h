/*
 * tasknc.h
 * by mjheagle
 */

#ifndef _TASKNC_H
#define _TASKNC_H

/* program description */
#define PROGNAME                        "tasknc"
#define PROGVERSION                     "v0.8"
#define PROGAUTHOR                      "mjheagle"

/* wiping functions */
#define wipe_tasklist()                 wipe_screen(tasklist, 0, rows-2)
#define wipe_statusbar()                wipe_screen(statusbar, 0, 0)

#define NVARS                           (int)(sizeof(vars)/sizeof(var))
#define NFUNCS                          (int)(sizeof(funcmaps)/sizeof(funcmap))

/* default settings */
#define STATUSBAR_TIMEOUT_DEFAULT       3
#define NCURSES_WAIT                    500
#define LOGLVL_DEFAULT                  3

/* function prototypes */
void check_resize();
void check_screen_size();
void cleanup();
void configure(void);
funcmap *find_function(const char *, const prog_mode);
void find_next_search_result(task *, task *);
var *find_var(const char *);
void force_redraw();
void handle_resize();
void help(void);
void key_command(const char *);
void key_task_background_command(const char *);
void key_task_interactive_command(const char *);
void key_done();
char max_project_length();
const char *name_function(void *);
void ncurses_end(int);
void ncurses_init();
void print_header();
void print_version(void);
void set_curses_mode(const ncurses_mode);
char *str_trim(char *);
int umvaddstr(WINDOW *, const int, const int, const char *, ...) __attribute__((format(printf,4,5)));
int umvaddstr_align(WINDOW *, const int, char *);
void wipe_screen(WINDOW *, const short, const short);
void wipe_window(WINDOW *);

#endif

// vim: noet ts=4 sw=4 sts=4
