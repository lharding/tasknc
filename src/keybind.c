/*
 * handle keybinds for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "keybind.h"

/*
 * keybind structure
 * represents an individual keybind
 */
struct keybind
{
        int key;
        bindfunc run;
        struct keybind *next;
};

/*
 * keybind_list structure
 * represents a list of keybinds
 */
struct keybind_list
{
        struct keybind *first;
        struct keybind *last;
        int count;
};

/*
 * named_function structure
 * represents a named function in the register of functions
 */
struct named_function
{
        char *name;
        bindfunc func;
        struct named_function *next;
};

/*
 * function_register structure
 * stores a list of named functions
 */
struct function_register
{
        struct named_function *first;
        struct named_function *last;
        int count;
};

/* create a new keybind list */
struct keybind_list *new_keybind_list() {
        struct keybind_list *new = calloc(1, sizeof(struct keybind_list));
        new->first = NULL;
        new->last = NULL;
        new->count = 0;

        return new;
}

/* add a keybind to the keybind list */
int add_keybind(struct keybind_list *list, int key, bindfunc run) {
        /* create a new keybind struct and assign fields */
        struct keybind *new = calloc(1, sizeof(struct keybind));
        if (!new)
                return 0;
        new->key = key;
        new->run = run;
        new->next = NULL;

        /* replace head if this is first keybind */
        if (list->first == NULL)
                list->first = new;

        /* append keybind to list */
        if (list->last != NULL)
                list->last->next = new;
        list->last = new;
        list->count++;

        return 1;
}

/* dump keybind list */
int dump_keybind_list(struct keybind_list *list) {
        if (list == NULL) {
                printf("null keybind list\n");
                return 0;
        }

        printf("keybinds: %d\n", list->count);
        struct keybind *head;
        for (head = list->first; head != NULL; head = head->next)
                printf("bind: '%d' to '%p'\n", head->key, head->run);

        return 1;
}

/* free keybind list */
void free_keybind_list(struct keybind_list *list) {
        if (list == NULL)
                return;

        /* iterate through keybinds and free them */
        struct keybind *head = list->first;
        while (head != NULL) {
                struct keybind *next = head->next;
                free(head);
                head = next;
        }

        /* free list */
        free(list);
}

/* evaluate a keybind */
int eval_keybind(struct keybind_list *list, int key, struct config *conf, struct task **tasks, struct nc_win *win) {
        int ret = -2;
        struct keybind *head;
        for (head = list->first; head != NULL; head = head->next) {
                if (head->key == key)
                        ret = (head->run)(conf, tasks, win);
                if (ret == 0)
                        break;
        }

        return ret;
}

/* create a new function register */
struct function_register *new_function_register() {
        struct function_register *new = calloc(1, sizeof(struct function_register));
        new->first = NULL;
        new->last = NULL;
        new->count = 0;

        return new;
}

/* add a function to the register */
int register_function(struct function_register *reg, char *name, bindfunc func) {
        /* create a new named_function struct and assign fields */
        struct named_function *new = calloc(1, sizeof(struct named_function));
        if (!new)
                return 0;
        new->name = strdup(name);
        new->func = func;
        new->next = NULL;

        /* replace head if this is first function registered */
        if (reg->first == NULL)
                reg->first = new;

        /* append function to list */
        if (reg->last != NULL)
                reg->last->next = new;
        reg->last = new;
        reg->count++;

        return 1;
}

/* dump registered functions */
int dump_function_register(struct function_register *reg) {
        if (reg == NULL) {
                printf("null function register\n");
                return 0;
        }

        printf("functions: %d\n", reg->count);
        struct named_function *head;
        for (head = reg->first; head != NULL; head = head->next)
                printf("func: '%s' to '%p'\n", head->name, head->func);

        return 1;
}

/* free function register */
void free_function_register(struct function_register *reg) {
        if (reg == NULL)
                return;

        /* iterate through named functions and free them */
        struct named_function *head = reg->first;
        while (head != NULL) {
                struct named_function *next = head->next;
                free(head->name);
                free(head);
                head = next;
        }

        /* free register */
        free(reg);
}
