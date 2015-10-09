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
    enum prog_mode mode;
    struct _bind* next;
} keybind;

void add_int_keybind(const int key,
                     void* function,
                     const int argint,
                     const enum prog_mode mode);

void add_keybind(const int key,
                 void* function,
                 char* arg,
                 const enum prog_mode mode);

void handle_keypress(const int c, const enum prog_mode mode);

char* name_key(const int val);

int parse_key(const char* keystr);

int remove_keybinds(const int key, const enum prog_mode mode);

extern FILE* logfp;
extern keybind* keybinds;

// vim: et ts=4 sw=4 sts=4
