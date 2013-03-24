/*
 * handle keybinds for tasknc
 * by mjheagle
 */

#include <stdio.h>
#include <stdlib.h>
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
