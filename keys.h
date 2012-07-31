/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * keys.h
 * for tasknc
 * by mjheagle
 */

#include "common.h"

typedef struct _bind
{
	int key;
	void (*function)();
	int argint;
	char *argstr;
	struct _bind *next;
	prog_mode mode;
} keybind;

void add_int_keybind(const int, void *, const int, const prog_mode);
void add_keybind(const int, void *, char *, const prog_mode);
char *name_key(const int);
int parse_key(const char *);
int remove_keybinds(const int);

extern FILE *logfp;
extern keybind *keybinds;
