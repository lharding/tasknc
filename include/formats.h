/*
 * formats.h
 * for tasknc
 * by mjheagle
 */

#ifndef _FIELDS_H
#define _FIELDS_H

#include "common.h"

fmt_field *compile_format_string(char *);
char *eval_format(fmt_field *, task *);
void compile_formats();
void free_formats();

#endif

// vim: noet ts=4 sw=4 sts=4
