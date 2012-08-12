/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * formats.h
 * for tasknc
 * by mjheagle
 */

#ifndef _FIELDS_H
#define _FIELDS_H

#include "common.h"

/* format field struct */
typedef struct _fmt_field
{
	var *variable;
	char *field;
	int length;
	int width;
} fmt_field;

fmt_field **compile_string();

#endif
