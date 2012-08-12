/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * formats.c - evaluating format strings
 * for tasknc
 * by mjheagle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "formats.h"

/* externs */
extern var *vars;

/* local functions */
static char *append_buffer(char *, const char, int *);
static fmt_field **append_field(fmt_field **, fmt_field *, int *);
static fmt_field *buffer_field(char *, int);

char *append_buffer(char *buffer, const char append, int *bufferlen) /* {{{ */
{
	/* append a character to a buffer */
	(*bufferlen)++;
	buffer = realloc(buffer, (*bufferlen)*sizeof(char));
	buffer[*bufferlen-1] = append;
	buffer[*bufferlen] = 0;

	return buffer;
} /* }}} */

fmt_field **append_field(fmt_field **fields, fmt_field *this, int *fieldcount) /* {{{ */
{
	/* append a field to the fields array */
	(*fieldcount)++;
	fields = realloc(fields, (*fieldcount)*sizeof(fmt_field));
	fields[*fieldcount-1] = this;
	fields[*fieldcount] = NULL;

	return fields;
} /* }}} */

fmt_field *buffer_field(char *buffer, int bufferlen) /* {{{ */
{
	/* create a format struct from a buffer string */
	fmt_field *this;

	this = calloc(1, sizeof(fmt_field));
	this->field = buffer;
	this->length = bufferlen;
	this->width = bufferlen;
	this->type = FIELD_STRING;

	return this;
} /* }}} */

fmt_field **compile_string(char *fmt) /* {{{ */
{
	/* compile a format string */
	fmt_field **fields = NULL, *this;
	int fieldcount = 0, buffersize = 0, i;
	char *buffer = NULL;
	static const char *task_field_map[] =
	{
		[FIELD_PROJECT]     = "project",
		[FIELD_DESCRIPTION] = "description",
		[FIELD_DUE]         = "due",
		[FIELD_PRIORITY]    = "priority",
		[FIELD_UUID]        = "uuid",
		[FIELD_INDEX]       = "index"
	};
	static const int ntask_fields = sizeof(task_field_map)/sizeof(char *);
	static const int nvars = sizeof(vars)/sizeof(var);

	/* check for an empty format string */
	if (fmt == NULL)
		return NULL;

	/* iterate through format string */
	while (*fmt != 0)
	{
		/* handle a character */
		if (*fmt != '$')
			buffer = append_buffer(buffer, *fmt, &buffersize);
		/* handle a variable */
		else
		{
			/* make existing buffer a field */
			if (buffer != NULL)
			{
				this = buffer_field(buffer, buffersize);
				buffer = NULL;
				buffersize = 0;
				fields = append_field(fields, this, &fieldcount);
			}
			fmt++;
			/* check for date */
			if (str_starts_with(fmt, "date"))
			{
				this = calloc(1, sizeof(fmt_field));
				this->type = FIELD_DATE;
				fields = append_field(fields, this, &fieldcount);
				fmt += 4;
				continue;
			}
			/* check for task field */
			for (i = 0; i < ntask_fields; i++)
			{
				if (str_starts_with(fmt, task_field_map[i]))
				{
					this = calloc(1, sizeof(fmt_field));
					this->type = FIELD_DATE;
					fields = append_field(fields, this, &fieldcount);
					fmt += strlen(task_field_map[i]);
					continue;
				}
			}
			/* check for a var */
			for (i = 0; i < nvars; i++)
			{
				if (vars[i].name == NULL)
					break;
				if (str_starts_with(fmt, vars[i].name))
				{
					this = calloc(1, sizeof(fmt_field));
					this->type = FIELD_VAR;
					this->variable = &(vars[i]);
					fmt += strlen(vars[i].name);
					continue;
				}
			}
		}
		fmt++;
	}
	/* handle a pending buffer */
	if (buffer != NULL)
	{
		this = buffer_field(buffer, buffersize);
		buffer = NULL;
		buffersize = 0;
		fields = append_field(fields, this, &fieldcount);
	}

	return fields;
} /* }}} */

char *eval_format(fmt_field **fmts, task *tsk) /* {{{ */
{
	/* evaluate a format field array */
	int nfmts = 0, totallen = 0, i, pos = 0, tmplen;
	char *str = NULL, *tmp;
	fmt_field *this;

	/* determine field length */
	while (fmts[nfmts] != NULL)
	{
		totallen += fmts[nfmts]->width;
		nfmts++;
	}

	/* build string */
	str = calloc(totallen, sizeof(char));
	for (i = 0; i < nfmts; i++)
	{
		this = fmts[i];
		switch(this->type)
		{
			case FIELD_STRING:
				strncpy(str+pos, this->field, this->width);
				break;
			case FIELD_DATE:
				tmp = utc_date(0);
				tmplen = strlen(tmp);
				strncpy(str+pos, tmp, tmplen);
				this->width = tmplen;
				break;
			default:
				break;
		}
		pos += this->width;
	}

	return str;
} /* }}} */
