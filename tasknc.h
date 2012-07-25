/*
 * header for tasknc
 * by mjheagle
 */

#ifndef _TASKNC_H
#define _TASKNC_H

/* wiping functions */
#define wipe_tasklist()                 wipe_screen(1, rows-2)
#define wipe_statusbar()                wipe_screen(rows-1, rows-1)

#define NVARS                           (int)(sizeof(vars)/sizeof(var))
#define NFUNCS                          (int)(sizeof(funcmaps)/sizeof(funcmap))

/* default settings */
#define STATUSBAR_TIMEOUT_DEFAULT       3
#define NCURSES_WAIT                    500
#define LOGLVL_DEFAULT                  3


/* function prototypes */
static void add_int_keybind(int, void *, int);
static void add_keybind(int, void *, char *);
static void check_curs_pos(void);
static void check_screen_size();
static void cleanup();
static void configure(void);
static const char *eval_string(const int, const char *, const task *, char *, int);
static funcmap *find_function(const char *);
static void find_next_search_result(task *, task *);
static var *find_var(const char *);
static void handle_command(char *);
static void handle_keypress(int);
static void help(void);
static void key_add();
static void key_command(const char *);
static void key_done();
static void key_filter(const char *);
static void key_modify(const char *);
static void key_reload();
static void key_scroll(const int);
static void key_search(const char *);
static void key_search_next();
static void key_sort();
static void key_sync();
static void key_task_action(const task_action_type, const char *, const char *);
static void key_undo();
static bool match_string(const char *, const char *);
static char max_project_length();
static void nc_colors(void);
static void nc_end(int);
static void nc_main();
static int parse_key(const char *);
static void print_task(int, task *);
static void print_task_list();
static void print_title();
static void print_version(void);
static int remove_keybinds(const int);
static void run_command_bind(char *);
static void run_command_unbind(char *);
static void run_command_set(char *);
static void run_command_show(const char *);
static void set_curses_mode(const ncurses_mode);
static void statusbar_message(const int, const char *, ...) __attribute__((format(printf,2,3)));
static char *str_trim(char *);
static int task_action(const task_action_type);
static void task_add(void);
static void task_count();
static bool task_match(const task *, const char *);
static void task_modify(const char *);
int umvaddstr(WINDOW *, const int, const int, const char *, ...) __attribute__((format(printf,4,5)));
int umvaddstr_align(WINDOW *, const int, char *);
static char *utc_date(const unsigned int);
static char *var_value_message(var *, bool);
static void wipe_screen(const short, const short);

#endif
