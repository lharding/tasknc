/*
 * sort.c - handle sorting of tasks
 * for tasknc
 * by mjheagle
 */

#include <string.h>
#include "common.h"
#include "sort.h"

/* local functions */
static bool compare_tasks(const struct task* a,
                          const struct task* b,
                          const char* mode_queue);

static int priority_to_int(const char pri);
static void sort_tasks(struct task* first, struct task* last);
static void swap_tasks(struct task* a, struct task* b);

bool compare_tasks(const struct task* a, const struct task* b,
                   const char* mode_queue) { /* {{{ */
    /**
     * compare two tasks to determine order
     * a return of 1 means that the tasks should be swapped (b comes before a)
     * a          - the first task to be compared
     * b          - the second task to be compared
     * mode_queue - the remaining tests to be evaluated
     */
    bool ret = false;
    int tmp, pri0, pri1;
    char sort_mode = *mode_queue;
    bool invert = false;

    /* check for inverted order */
    if (sort_mode >= 'A' && sort_mode <= 'Z') {
        sort_mode += 32;
        invert = true;
    }

    /* determine sort algorithm and apply it */
    switch (sort_mode) {
    case 'n':       // sort by index
        if (XOR(invert, a->index < b->index)) {
            ret = true;
        }

        break;

    case 'p':       // sort by project name
        if (a->project == NULL) {
            if (b->project != NULL) {
                ret = true;
            }

            break;
        }

        if (b->project == NULL) {
            break;
        }

        tmp = strcmp(a->project, b->project);

        if (XOR(invert, tmp < 0)) {
            ret = true;
        }

        if (tmp == 0) {
            ret = compare_tasks(a, b, mode_queue + 1);
        }

        break;

    case 'd':       // sort by due date
        if (a->due == 0) {
            if (b->due == 0) {
                ret = compare_tasks(a, b, mode_queue + 1);
            }

            break;
        } else if (b->due == 0) {
            ret = true;
            break;
        } else if (XOR(invert, a->due < b->due)) {
            ret = true;
        }

        break;

    case 'r':       // sort by priority
        pri0 = priority_to_int(a->priority);
        pri1 = priority_to_int(b->priority);

        if (pri0 == pri1) {
            ret = compare_tasks(a, b, mode_queue + 1);
        } else {
            ret = XOR(invert, pri0 > pri1);
        }

        break;

    case 'u':       // sort by uuid
        if (XOR(invert, strcmp(a->uuid, b->uuid) < 0)) {
            ret = true;
        }

        break;

    case 0:
    default:
        return false;
        break;
    }

    return ret;
} /* }}} */

int priority_to_int(const char pri) { /* {{{ */
    /* map a priority to a number */
    switch (pri) {
    case 'H':
        return 3;
        break;

    case 'M':
        return 2;
        break;

    case 'L':
        return 1;
        break;

    default:
    case 0:
        return 0;
        break;
    }
} /* }}} */

void sort_wrapper(struct task* first) { /* {{{ */
    /**
     * a wrapper around sort_tasks that finds the last element
     * in the linked list to pass to the aforementioned function
     */
    struct task* last;

    /* loop through looking for last item */
    last = first;

    while (last->next != NULL) {
        last = last->next;
    }

    /* run sort with last value */
    sort_tasks(first, last);
} /* }}} */

void sort_tasks(struct task* first, struct task* last) { /* {{{ */
    /* sort a list of tasks from first to last */
    struct task* start, *cur, *oldcur;

    /* check if we are done */
    if (first == last) {
        return;
    }

    /* set start and current */
    start = first;
    cur = start->next;

    /* iterate through to right end, sorting as we go */
    while (1) {
        if (compare_tasks(start, cur, cfg.sortmode)) {
            swap_tasks(start, cur);
        }

        if (cur == last) {
            break;
        }

        cur = cur->next;
    }

    /* swap first and cur */
    swap_tasks(first, cur);

    /* save this cur */
    oldcur = cur;

    /* sort left side */
    cur = cur->prev;

    if (cur != NULL) {
        if ((first->prev != cur) && (cur->next != first)) {
            sort_tasks(first, cur);
        }
    }

    /* sort right side */
    cur = oldcur->next;

    if (cur != NULL) {
        if ((cur->prev != last) && (last->next != cur)) {
            sort_tasks(cur, last);
        }
    }
} /* }}} */

void swap_tasks(struct task* a, struct task* b) { /* {{{ */
    /* swap the contents of two tasks */
    unsigned short ustmp;
    unsigned int uitmp;
    char* strtmp;
    char ctmp;

    ustmp = a->index;
    a->index = b->index;
    b->index = ustmp;

    strtmp = a->uuid;
    a->uuid = b->uuid;
    b->uuid = strtmp;

    strtmp = a->tags;
    a->tags = b->tags;
    b->tags = strtmp;

    uitmp = a->start;
    a->start = b->start;
    b->start = uitmp;

    uitmp = a->end;
    a->end = b->end;
    b->end = uitmp;

    uitmp = a->entry;
    a->entry = b->entry;
    b->entry = uitmp;

    uitmp = a->due;
    a->due = b->due;
    b->due = uitmp;

    strtmp = a->project;
    a->project = b->project;
    b->project = strtmp;

    ctmp = a->priority;
    a->priority = b->priority;
    b->priority = ctmp;

    strtmp = a->description;
    a->description = b->description;
    b->description = strtmp;
} /* }}} */

// vim: et ts=4 sw=4 sts=4
