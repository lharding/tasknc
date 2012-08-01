/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * tasknc.h
 * by mjheagle
 */

#ifndef _TASKncurses_H
#define _TASKncurses_H

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
void check_screen_size();
void cleanup();
void configure(void);
const char *eval_string(const int, char *, const task *, char *, int);
funcmap *find_function(const char *, const prog_mode);
void find_next_search_result(task *, task *);
var *find_var(const char *);
void force_redraw();
void handle_command(char *);
void help(void);
void key_command(const char *);
void key_done();
char max_project_length();
const char *name_function(void *);
void ncurses_colors(void);
void ncurses_end(int);
void ncurses_init();
void print_header();
void print_version(void);
void run_command_bind(char *);
void run_command_unbind(char *);
void run_command_set(char *);
void run_command_show(const char *);
void set_curses_mode(const ncurses_mode);
void statusbar_message(const int, const char *, ...) __attribute__((format(printf,2,3)));
void statusbar_timeout();
char *str_trim(char *);
int umvaddstr(WINDOW *, const int, const int, const char *, ...) __attribute__((format(printf,4,5)));
int umvaddstr_align(WINDOW *, const int, char *);
char *utc_date(const unsigned int);
char *var_value_message(var *, bool);
void wipe_screen(WINDOW *, const short, const short);
void wipe_window(WINDOW *);

#endif
