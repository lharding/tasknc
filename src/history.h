/**
 * @file history.h
 * @author mjheagle
 * @brief command history management for tasknc
 */

#ifndef _TASKNC_HISTORY_H
#define _TASKNC_HISTORY_H

#include <wchar.h>
#include "configure.h"

/**
 * opaque representation of cmd_history struct
 */
struct cmd_history;

/**
 * create a cmd_history struct
 *
 * @param conf the configuration struct
 *
 * @return an allocated cmd_history struct
 */
struct cmd_history *init_history(struct config *conf);

/**
 * free a cmd_history struct
 *
 * @param hist the cmd_history struct to free
 */
void free_cmd_history(struct cmd_history *hist);

/**
 * add a history entry
 *
 * @param hist the cmd_history struct to add to
 * @param prompt the prompt that was used
 * @param entry the entry to add
 */
void add_history_entry(struct cmd_history *hist, wchar_t *prompt, wchar_t *entry);

/**
 * get a history entry
 *
 * @param hist the cmd_history struct to retrieve from
 * @param prompt the prompt whose history to search
 * @param num the number of the history entry to fetch
 *
 * @return a string history entry
 */
wchar_t *get_history(struct cmd_history *hist, wchar_t *prompt, int num);

#endif
