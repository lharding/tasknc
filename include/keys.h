/*
 * keys.h
 * for tasknc
 * by mjheagle
 */

#include "common.h"

/**
 * keybind structure
 * key      - the key which triggers the bind
 * function - the function to be run on bind trigger
 * argint   - integer argument to be supplied to function
 * argstr   - string argument to be supplied to function
 * mode     - which mode this keybind should run in
 * next     - a pointer to the next keybind
 */
typedef struct _bind {
    int key;
    void (*function)();
    int argint;
    char* argstr;
    prog_mode mode;
    struct _bind* next;
} keybind;

void add_int_keybind(const int, void*, const int, const prog_mode);
void add_keybind(const int, void*, char*, const prog_mode);
void handle_keypress(const int, const prog_mode);
char* name_key(const int);
int parse_key(const char*);
int remove_keybinds(const int, const prog_mode);

extern FILE* logfp;
extern keybind* keybinds;

// vim: noet ts=4 sw=4 sts=4
