/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * formats.c - evaluating format strings
 * for tasknc
 * by mjheagle
 */

#include <stdlib.h>
#include "common.h"
#include "formats.h"

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

	return this;
} /* }}} */

fmt_field **compile_string(char *fmt) /* {{{ */
{
	/* compile a format string */
	fmt_field **fields = NULL, *this;
	int fieldcount = 0, buffersize = 0;
	char *buffer = NULL;

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
				append_field(fields, this, &fieldcount);
			}
		}
		fmt++;
	}
	/* handle a pending buffer */
	if (buffer != NULL)
	{
		this = buffer_field(buffer, buffersize);
		fields = append_field(fields, this, &fieldcount);
	}

	return fields;
} /* }}} */
