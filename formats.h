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
	fmt_field_type type;
	var *variable;
	char *field;
	unsigned int length;
	unsigned int width;
	bool right_align;
} fmt_field;

fmt_field **compile_string();
char *eval_format(fmt_field **, task *);

#endif
