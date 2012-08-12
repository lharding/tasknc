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

/* special fields */
typedef enum
{
	FIELD_DATE = 0,
	FIELD_PROJECT,
	FIELD_DESCRIPTION,
	FIELD_DUE,
	FIELD_PRIORITY,
	FIELD_UUID,
	FIELD_INDEX,
	FIELD_STRING,
	FIELD_VAR
} fmt_field_type;

/* format field struct */
typedef struct _fmt_field
{
	var *variable;
	char *field;
	int length;
	int width;
	fmt_field_type type;
} fmt_field;

fmt_field **compile_string();
char *eval_format(fmt_field **, task *);

extern var *vars;

#endif
