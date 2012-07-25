/*
 * tasknc.h
 * by mjheagle
 *
 * vim: noet ts=4 sw=4 sts=4
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
void check_curs_pos(void);
void check_screen_size();
void cleanup();
void configure(void);
const char *eval_string(const int, const char *, const task *, char *, int);
funcmap *find_function(const char *);
void find_next_search_result(task *, task *);
var *find_var(const char *);
void handle_command(char *);
void handle_keypress(int);
void help(void);
void key_command(const char *);
void key_tasklist_action(const task_action_type, const char *, const char *);
void key_tasklist_add();
void key_tasklist_done();
void key_tasklist_filter(const char *);
void key_tasklist_modify(const char *);
void key_tasklist_reload();
void key_tasklist_scroll(const int);
void key_tasklist_search(const char *);
void key_tasklist_search_next();
void key_tasklist_sort();
void key_tasklist_sync();
void key_tasklist_undo();
char max_project_length();
void ncurses_colors(void);
void ncurses_end(int);
void print_header();
void print_version(void);
void run_command_bind(char *);
void run_command_unbind(char *);
void run_command_set(char *);
void run_command_show(const char *);
void set_curses_mode(const ncurses_mode);
void statusbar_message(const int, const char *, ...) __attribute__((format(printf,2,3)));
char *str_trim(char *);
void tasklist_print_task(int, task *);
void tasklist_print_task_list();
int tasklist_task_action(const task_action_type);
void tasklist_task_add(void);
void tasklist_window();
int umvaddstr(WINDOW *, const int, const int, const char *, ...) __attribute__((format(printf,4,5)));
int umvaddstr_align(WINDOW *, const int, char *);
char *utc_date(const unsigned int);
char *var_value_message(var *, bool);
void wipe_screen(WINDOW *, const short, const short);

#endif
