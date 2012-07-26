/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * keys.h
 * for tasknc
 * by mjheagle
 */

#include "common.h"

void add_int_keybind(int, void *, int);
void add_keybind(int, void *, char *);
int parse_key(const char *);
int remove_keybinds(const int);

extern FILE *logfp;
