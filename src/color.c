/*
 * color.c - manage curses colors
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "color.h"
#include "common.h"
#include "log.h"
#include "tasks.h"

/**
 * color structure
 * pair - the color pair number to be passed to COLOR_PAIR
 * fg   - the number of the foreground color
 * bg   - the number of the background color
 */
struct color {
    short pair;
    short fg;
    short bg;
};

/**
 * color rule structure
 * pair   - the color pair number to be passed to COLOR_PAIR
 * rule   - the string containing the rule to be evaluated
 * object - the type of item that is being colored
 * next   - the next color_rule struct
 */
struct color_rule {
    short pair;
    char* rule;
    enum color_object object;
    struct color_rule* next;
};

/* global variables */
bool use_colors;
bool colors_initialized = false;
bool* pairs_used = NULL;
struct color_rule* color_rules = NULL;

/* local functions */
static short add_color_pair(const short askpair,
                            const short fg,
                            const short bg);

int check_color(int color);

static bool eval_rules(char* rule,
                       const struct task*,
                       const bool selected);

static short find_add_pair(const short fg,
                           const short bg);

static int set_default_colors(void);

short add_color_pair(short askpair, short fg, short bg) { /* {{{ */
    /**
     * initialize a color pair and return its pair number
     * askpair - the pair number being requested (<=0 for next free)
     * fg      - the foreground color to be stored
     * bg      - the background color to be stored
     */
    short pair = 1;

    /* pick a color number if none is specified */
    if (askpair <= 0) {
        while (pair < COLOR_PAIRS && pairs_used[pair]) {
            pair++;
        }

        if (pair == COLOR_PAIRS) {
            return -1;
        }
    }

    /* check if pair requested is being used */
    else {
        if (pairs_used[askpair]) {
            return -1;
        }

        pair = askpair;
    }

    /* initialize pair */
    if (init_pair(pair, fg, bg) == ERR) {
        return -1;
    }

    /* mark pair as used and exit */
    pairs_used[pair] = true;
    tnc_fprintf(logfp, LOG_DEBUG, "assigned color pair %hd to (%hd, %hd)",
                pair, fg, bg);
    return pair;
} /* }}} */

short add_color_rule(const enum color_object object,
                     const char* rule,
                     const short fg,
                     const short bg) { /* {{{ */
    /**
     * add or overwrite a color rule for the provided conditions
     * if the rule is unique, create a new rule
     * if the rule is not unique, overwrite the colors being set
     *
     * object - the type of object this rule applies to
     * rule   - a string containing the rule to be evaluated
     * fg     - the foreground color to be set when rule is true
     * bg     - the background color to be set when rule is true
     */
    struct color_rule*  last;
    struct color_rule*  this;
    short               ret;
    struct task*        tsk;

    /* reset color caches if past configuration */
    if (tasklist != NULL) {
        tsk = head;

        while (tsk != NULL) {
            tsk->selpair = -1;
            tsk->pair = -1;
            tsk = tsk->next;
        }
    }

    /* look for existing rule and overwrite colors */
    this = color_rules;
    last = color_rules;

    while (this != NULL) {
        if (this->object == object && (this->rule == rule || (this->rule != NULL &&
                                       rule != NULL && strcmp(this->rule, rule) == 0))) {
            ret = find_add_pair(fg, bg);

            if (ret < 0) {
                return ret;
            }

            this->pair = ret;
            return 0;
        }

        last = this;
        this = this->next;
    }

    /* or create a new rule */
    ret = find_add_pair(fg, bg);

    if (ret < 0) {
        return ret;
    }

    this = calloc(1, sizeof(struct color_rule));
    this->pair = ret;

    if (rule != NULL) {
        this->rule = strdup(rule);
    } else {
        this->rule = NULL;
    }

    this->object = object;
    this->next = NULL;

    if (last != NULL) {
        last->next = this;
    } else {
        color_rules = this;
    }

    return 0;
} /* }}} */

int check_color(int color) { /* {{{ */
    /**
     * make sure a color is valid before using it
     * return -1 (default) if color is not valid
     */
    if (color >= COLORS || color < -2) {
        return -1;
    } else {
        return color;
    }
} /* }}} */

bool eval_rules(char* rule, const struct task* tsk, const bool selected) { /* {{{ */
    /**
     * evaluate a rule set for a task
     * rule     - the rule to be evaluated
     * tsk      - the task the rule will be evaluated on
     * selected - whether the task is selected
     */
    char*   regex = NULL;
    char    pattern;
    char*   tmp;
    int     ret;
    int     move;
    bool    go = false;
    bool    invert = false;

    /* success if rules are done */
    if (rule == NULL || *rule == 0) {
        return true;
    }

    /* skip non-patterns */
    if (*rule != '~') {
        return eval_rules(rule + 1, tsk, selected);
    }

    /* regex match */
    ret = sscanf(rule, "~%c '%m[^\']'", &pattern, &regex);

    if (ret > 0 && pattern >= 'A' && pattern <= 'Z') {
        pattern += 32;
        invert = true;
    }

    if (ret == 1) {
        switch (pattern) {
        case 's':
            if (!XOR(invert, selected)) {
                return false;
            } else {
                return eval_rules(rule + 2, tsk, selected);
            }

            break;

        case 't':
            if (!XOR(invert, tsk->start > 0)) {
                return false;
            } else {
                return eval_rules(rule + 2, tsk, selected);
            }

            break;

        default:
            break;
        }
    }

    if (ret == 2) {
        move = strlen(regex) + 3;
        go = true;

        switch (pattern) {
        case 'p':
            if (!XOR(invert, match_string(tsk->project, regex))) {
                return false;
            } else {
                tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "eval_rules: project match - '%s' '%s'",
                            tsk->project, regex);
            }

            break;

        case 'd':
            if (!XOR(invert, match_string(tsk->description, regex))) {
                return false;
            } else {
                tnc_fprintf(logfp, LOG_DEBUG_VERBOSE,
                            "eval_rules: description match - '%s' '%s'", tsk->description, regex);
            }

            break;

        case 't':
            if (!XOR(invert, match_string(tsk->tags, regex))) {
                return false;
            } else {
                tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "eval_rules: tag match - '%s' '%s'",
                            tsk->tags, regex);
            }

            break;

        case 'r':
            tmp = calloc(2, sizeof(char));
            *tmp = tsk->priority;

            if (!XOR(invert, match_string(tmp, regex))) {
                return false;
            } else {
                tnc_fprintf(logfp, LOG_DEBUG_VERBOSE, "eval_rules: priority match - '%s' '%s'",
                            tmp, regex);
            }

            free(tmp);
            break;

        default:
            go = false;
            break;
        }

        free(regex);

        if (go) {
            return eval_rules(rule + move, tsk, selected);
        }
    }

    /* should never get here */
    tnc_fprintf(logfp, LOG_ERROR, "malformed rules - \"%s\"", rule);
    return false;
} /* }}} */

short find_add_pair(const short fg, const short bg) { /* {{{ */
    /**
     * find a color pair with specified content or create a new one
     * fg - the foreground color
     * bg - the background color
     */
    short tmpfg;
    short tmpbg;
    int pair;
    int free_pair = -1;
    int   ret;

    /* look for an existing pair */
    for (pair = 1; pair < COLOR_PAIRS; pair++) {
        if (pairs_used[pair]) {
            ret = pair_content(pair, &tmpfg, &tmpbg);

            if (ret == ERR) {
                continue;
            }

            if (tmpfg == fg && tmpbg == bg) {
                return pair;
            }
        } else if (free_pair == -1) {
            free_pair = pair;
        }
    }

    /* return a new pair */
    return add_color_pair(free_pair, fg, bg);
} /* }}} */

void free_colors(void) { /* {{{ */
    /* clean up memory allocated for colors */
    struct color_rule* this;
    struct color_rule* last;

    check_free(pairs_used);

    this = color_rules;

    while (this != NULL) {
        last = this;
        this = this->next;
        check_free(last->rule);
        free(last);
    }
} /* }}} */

int get_colors(const enum color_object object,
               struct task* tsk,
               const bool selected) { /* {{{ */
    /**
     * evaluate color rules and return an argument to attrset
     * object   - the object to be colored
     * tsk      - the task to be colored
     * selected - whether the task is selected
     */
    short               pair = 0;
    int*                tskpair;
    struct color_rule*  rule;
    bool                done = false;

    /* check for cache if task */
    if (object == OBJECT_TASK) {
        if (selected) {
            tskpair = &(tsk->selpair);
        } else {
            tskpair = &(tsk->pair);
        }

        if (*tskpair >= 0) {
            return COLOR_PAIR(*tskpair);
        }
    }

    /* iterate through rules */
    rule = color_rules;

    while (rule != NULL) {
        /* check for matching object */
        if (object == rule->object) {
            switch (object) {
            case OBJECT_ERROR:
            case OBJECT_HEADER:
                done = true;
                break;

            case OBJECT_TASK:
                if (eval_rules(rule->rule, tsk, selected)) {
                    pair = rule->pair;
                }

                break;

            default:
                break;
            }
        }

        if (done) {
            pair = rule->pair;
            break;
        }

        rule = rule->next;
    }

    /* assign cached color if task object */
    if (object == OBJECT_TASK) {
        *tskpair = (int)pair;
    }

    return COLOR_PAIR(pair);
} /* }}} */

int init_colors(void) { /* {{{ */
    /* initialize curses colors */
    int ret;
    use_colors = false;

    /* attempt to start colors */
    ret = start_color();

    if (ret == ERR) {
        return 1;
    }

    /* apply default colors */
    ret = use_default_colors();

    if (ret == ERR) {
        return 2;
    }

    /* check if terminal has color capabilities */
    use_colors = has_colors();
    colors_initialized = true;

    if (use_colors) {
        /* allocate pairs_used */
        pairs_used = calloc(COLOR_PAIRS, sizeof(bool));
        pairs_used[0] = true;

        return set_default_colors();
    } else {
        return 3;
    }
} /* }}} */

int parse_color(const char* name) { /* {{{ */
    /* parse a color from a string */
    unsigned int    i;
    int             ret;
    struct color_map {
        const int   color;
        const char* name;
    };

    /* color map */
    static const struct color_map colors_map[] = {
        {COLOR_BLACK,   "black"},
        {COLOR_RED,     "red"},
        {COLOR_GREEN,   "green"},
        {COLOR_YELLOW,  "yellow"},
        {COLOR_BLUE,    "blue"},
        {COLOR_MAGENTA, "magenta"},
        {COLOR_CYAN,    "cyan"},
        {COLOR_WHITE,   "white"},
    };

    /* try for int */
    ret = sscanf(name, "%d", &i);

    if (ret == 1) {
        return check_color(i);
    }

    /* try for colorNNN */
    ret = sscanf(name, "color%3d", &i);

    if (ret == 1) {
        return check_color(i);
    }

    /* look for mapped color */
    for (i = 0; i < sizeof(colors_map) / sizeof(struct color_map); i++) {
        if (str_eq(colors_map[i].name, name)) {
            return check_color(colors_map[i].color);
        }
    }

    return -2;
} /* }}} */

enum color_object parse_object(const char* name) { /* {{{ */
    /* parse an object from a string */
    unsigned int i;
    struct color_object_map {
        const enum color_object object;
        const char*             name;
    };

    /* object map */
    static const struct color_object_map color_objects_map[] = {
        {OBJECT_HEADER, "header"},
        {OBJECT_TASK,   "task"},
        {OBJECT_ERROR,  "error"},
    };

    /* evaluate map */
    for (i = 0; i < sizeof(color_objects_map) / sizeof(struct color_object_map);
         i++) {
        if (str_eq(color_objects_map[i].name, name)) {
            return color_objects_map[i].object;
        }
    }

    return OBJECT_NONE;
} /* }}} */

int set_default_colors(void) { /* {{{ */
    /* create initial color rules */
    add_color_rule(OBJECT_HEADER, NULL, COLOR_BLUE, COLOR_BLACK);
    add_color_rule(OBJECT_TASK, NULL, -1, -1);
    add_color_rule(OBJECT_TASK, "~s", COLOR_CYAN, COLOR_BLACK);
    add_color_rule(OBJECT_ERROR, NULL, COLOR_RED, -1);

    return 0;
} /* }}} */

// vim: et ts=4 sw=4 sts=4
