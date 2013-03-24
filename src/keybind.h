/**
 * @file keybind.h
 * @author mjheagle
 * @brief key bindings for tasknc
 */

#ifndef _TASKNC_KEYBIND_H
#define _TASKNC_KEYBIND_H

#include "configure.h"
#include "task.h"

/**
 * keybind function typedef
 */
typedef int (*bindfunc)(struct config *, struct task **);

/**
 * opaque representation of keybind list struct
 */
struct keybind_list;

/**
 * create a new keybind list
 *
 * @return allocated task list
 */
struct keybind_list *new_keybind_list();

/**
 * add a new keybind to a keybind list
 *
 * @param list the keybind list to append to
 * @param key the key to be bound
 * @param bindfunc the function to run when the bound key is pressed
 *
 * @return an indicator of success
 */
int add_keybind(struct keybind_list *list, int key, bindfunc run);

/**
 * dump the contents of a keybind list
 *
 * @param list the keybind list to dump
 *
 * @return an indicator of success
 */
int dump_keybind_list(struct keybind_list *list);

/**
 * free an allocated keybind list
 *
 * @param list the keybind list to free
 */
void free_keybind_list(struct keybind_list *list);

#endif
