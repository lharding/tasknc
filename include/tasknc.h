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

#define NVARS                           (int)(sizeof(vars)/sizeof(struct var))
#define NFUNCS                          (int)(sizeof(funcmaps)/sizeof(funcmap))

/* default settings */
#define STATUSBAR_TIMEOUT_DEFAULT       3
#define NCURSES_WAIT                    500
#define LOGLVL_DEFAULT                  3

/* function prototypes */
void check_resize(void);
void check_screen_size(void);
void cleanup(void);
void configure(void);
funcmap* find_function(const char* name, const prog_mode mode);
void find_next_search_result(task* head, task* pos);
struct var* find_var(const char* name);
void force_redraw(void);
void handle_resize(void);
void help(void);
void key_command(const char* arg);
void key_task_background_command(const char* arg);
void key_task_interactive_command(const char* arg);
void key_done(void);
char max_project_length(void);
const char* name_function(void*);
void ncurses_end(int);
void ncurses_init(void);
void print_header(void);
void print_version(void);
void set_curses_mode(const enum ncurses_mode mode);
char* str_trim(char* str);

int umvaddstr(WINDOW* win,
              const int y,
              const int x,
              const char* format,
              ...) __attribute__((format(printf, 4, 5)));

int umvaddstr_align(WINDOW* win, const int y, char* str);

void wipe_screen(WINDOW* win,
                 const short startl,
                 const short stopl);

void wipe_window(WINDOW* win);

#endif

// vim: et ts=4 sw=4 sts=4
