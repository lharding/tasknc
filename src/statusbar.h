/**
 * @file statusbar.h
 * @author mjheagle
 * @brief statusbar window for tasknc
 */

#ifndef _TASKNC_STATUSBAR_H
#define _TASKNC_STATUSBAR_H

#include "configure.h"
#include "cursutil.h"

/**
 * display a message in the statusbar
 *
 * @param bar window to print to
 * @param conf configuration struct pointer
 * @param format the printf format string to evaluate
 *
 * @return indicator of success
 */
int statusbar_printf(struct nc_win * bar, struct config * conf, const char *format, ...);

/**
 * wipe the statusbar
 *
 * @param arg the bindarg struct
 *
 * @return indicator of success
 */
int statusbar_clear(struct bindarg * arg);

#endif
