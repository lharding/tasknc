/*
 * command history management for tasknc
 * by mjheagle
 */

#include <stdio.h>
#include <stdlib.h>
#include "history.h"

/* cmd_history container */
struct cmd_history {
        struct cmd_history_entry *last;
        struct prompt_history *prompts;
};

/* cmd_history entry */
struct cmd_history_entry {
        struct cmd_history_entry *prev;
        struct cmd_history_entry *next;
        int prompt_index;
        wchar_t *entry;
};

/* prompt history */
struct prompt_history {
        struct prompt_history *next;
        wchar_t *prompt;
        int index;
};

/* initialize a new cmd_history struct */
struct cmd_history *init_history(struct config *conf) {
        struct cmd_history *new = calloc(1, sizeof(struct cmd_history));

        if (new != NULL) {
                new->last = NULL;
        }

        return new;
}

/* free a cmd_history struct */
void free_cmd_history(struct cmd_history *hist) {
        /* free history entries */
        struct cmd_history_entry *this = hist->last;
        while (this != NULL) {
                struct cmd_history_entry *next = this->prev;
                if (this->entry != NULL)
                        free(this->entry);
                free(this);
                this = next;
        }

        /* free prompt history */
        struct prompt_history *phist = hist->prompts;
        while (phist != NULL) {
                struct prompt_history *next = phist->next;
                if (phist->prompt != NULL)
                        free(phist->prompt);
                free(phist);
                phist = next;
        }

        /* free history container */
        free(hist);
}

/* get prompt index */
int get_prompt_index(struct cmd_history *hist, wchar_t *prompt) {
        struct prompt_history *this = hist->prompts;

        /* find prompt in existing history */
        while (this != NULL) {
                if (wcscmp(this->prompt, prompt) == 0)
                        return this->index;
                this = this->next;
        }

        /* create new history entry */
        this = calloc(1, sizeof(struct prompt_history));
        this->index = hist->prompts == NULL ? 0 : hist->prompts->index + 1;
        this->next = hist->prompts;
        hist->prompts = this;
        this->prompt = wcsdup(prompt);

        return this->index;
}

/* add a history entry */
void add_history_entry(struct cmd_history *hist, wchar_t *prompt, wchar_t *entry) {
        struct cmd_history_entry *new = calloc(1, sizeof(struct cmd_history_entry));
        new->prompt_index = get_prompt_index(hist, prompt);
        new->entry = wcsdup(entry);
        new->prev = hist->last;
        new->next = NULL;
        if (hist->last != NULL)
                hist->last->next = new;
        hist->last = new;
}

/* get history index */
wchar_t *get_history(struct cmd_history *hist, wchar_t *prompt, int num) {
        const int prompt_index = get_prompt_index(hist, prompt);
        struct cmd_history_entry *this;

        for (this = hist->last; this != NULL; this = this->prev) {
                if (this->prompt_index == prompt_index)
                        num--;
                if (num < 0)
                        return this->entry;
        }

        return NULL;
}
