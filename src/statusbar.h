/**
 * @file statusbar.h
 * @author mjheagle
 * @brief statusbar window for tasknc
 */

#ifndef _TASKNC_STATUSBAR_H
#define _TASKNC_STATUSBAR_H

/**
 * display a message in the statusbar
 *
 * @param arg bindarg structure
 * @param format the printf format string to evaluate
 *
 * @return indicator of success
 */
int statusbar_printf(struct bindarg * arg, const char *format, ...);

/**
 * wipe the statusbar
 *
 * @param arg the bindarg struct
 *
 * @return indicator of success
 */
int statusbar_clear(struct bindarg * arg);

/**
 * get string input from the statusbar
 *
 * @param arg the bindarg struct
 * @param msg the prefix to input
 * @param str a pointer to the memory where the string will be stored
 *
 * @return indicator of success
 */
int statusbar_get_string(struct bindarg * arg, const char *msg, char **str);

#endif
