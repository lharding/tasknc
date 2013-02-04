/**
 * @file cursutil.h
 * @author mjheagle
 * @brief ncurses utilities for tasknc
 */


#ifndef _TASKNC_CURSUTIL_H
#define _TASKNC_CURSUTIL_H

#include <curses.h>

/**
 * evaluate a 'char *' format string, convert to 'wchar_t *' and print to ncurses
 * window
 *
 * @param win ncurses window to print in
 * @param y y coordinate to print at
 * @param x x coordinate to begin printing at
 * @param width maximum number of characters to print
 * @param format printf style string to evaluate with subsequent arguments
 *
 * @return an indicator of success, either ERR or OK
 */
int umvaddstr(WINDOW *win, const int y, const int x, const int width, const char *format, ...);

/**
 * split a string at %> and print the sections left and right aligned
 *
 * @param win ncurses window to print in
 * @param y y coordinate to print at
 * @param width maximum number of characters to print
 * @param str string to parse and print
 *
 * @return an indicator of success, either ERR or OK
 */
int umvaddstr_align(WINDOW *win, const int y, const int width, char *str);

/**
 * ncurses input modes
 */
enum ncurses_mode {
        NCURSES_MODE_STD,               /**< non-blocking cbreak input */
        NCURSES_MODE_STD_BLOCKING,      /**< blocking cbreak input */
        NCURSES_MODE_STRING             /**< blocking non-cbreak input */
};

/**
 * set ncurses input mode
 *
 * @param win ncurses window that will be handling input
 * @param mode ncurses mode describing input parameters.
 *
 * @todo get timeout from config
 */
void set_curses_mode(WINDOW *win, const enum ncurses_mode mode);

/**
 * ncurses window types
 */
enum ncurses_window_type {
        TASKNC_TASKLIST,        /**< tasklist for displaying tasks */
        TASKNC_STATUSLINE,      /**< statusline for displaying messages and
                                  getting user input */
        TASKNC_HEADER,          /**< header window */
        TASKNC_PAGER,           /**< window for displaying miscellaneous data */
};

/**
 * ncurses window data container
 */
struct nc_win {
        WINDOW *win;                    /**< ncurses window pointer */
        enum ncurses_window_type type;  /**< window type */
        int height;                     /**< window height */
        int offset;                     /**< lines window is offset */
        int nlines;                     /**< number of lines of data to display */
        int selline;                    /**< selected line number */
};

#endif
