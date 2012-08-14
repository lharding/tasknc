/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * formats.c - evaluating format strings
 * for tasknc
 * by mjheagle
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "formats.h"

/* externs */
extern var vars[];

/* local functions */
static char *append_buffer(char *, const char, int *);
static fmt_field **append_field(fmt_field **, fmt_field *, int *);
static fmt_field *buffer_field(char *, int);
static char *field_to_str(fmt_field *, bool *, task *);
static conditional_fmt_field *parse_conditional(char *, int *);

char *append_buffer(char *buffer, const char append, int *bufferlen) /* {{{ */
{
	/* append a character to a buffer */
	(*bufferlen)++;
	buffer = realloc(buffer, ((*bufferlen)+1)*sizeof(char));
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
	int fieldcount = 0, buffersize = 0, i, width;
	char *buffer = NULL;
	bool next, right_align;
	static const char *task_field_map[] =
	{
		[FIELD_PROJECT]     = "project",
		[FIELD_DESCRIPTION] = "description",
		[FIELD_DUE]         = "due",
		[FIELD_PRIORITY]    = "priority",
		[FIELD_UUID]        = "uuid",
		[FIELD_INDEX]       = "index"
	};

	/* check for an empty format string */
	if (fmt == NULL)
		return NULL;

	/* iterate through format string */
	while (*fmt != 0)
	{
		next = false;
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
			/* check for right align */
			right_align = *fmt == '-';
			if (right_align)
				fmt++;
			/* check for width specification */
			width = 0;
			while (*fmt >= '0' && *fmt <= '9')
				width = (10 * width) + (*(fmt++) - 48);
			/* check for date */
			if (str_starts_with(fmt, "date"))
			{
				this = calloc(1, sizeof(fmt_field));
				this->type = FIELD_DATE;
				this->width = width;
				this->right_align = right_align;
				fields = append_field(fields, this, &fieldcount);
				fmt += 4;
				continue;
			}
			/* check for task field */
			for (i = FIELD_PROJECT; i < FIELD_INDEX; i++)
			{
				if (str_starts_with(fmt, task_field_map[i]))
				{
					this = calloc(1, sizeof(fmt_field));
					this->type = i;
					this->width = width;
					this->right_align = right_align;
					fields = append_field(fields, this, &fieldcount);
					fmt += strlen(task_field_map[i]);
					next = true;
					break;
				}
			}
			if (next)
				continue;
			/* check for a var */
			for (i = 0; vars[i].name != NULL; i++)
			{
				if (str_starts_with(fmt, vars[i].name))
				{
					this = calloc(1, sizeof(fmt_field));
					this->width = width;
					this->right_align = right_align;
					this->type = FIELD_VAR;
					this->variable = &(vars[i]);
					fields = append_field(fields, this, &fieldcount);
					fmt += strlen(vars[i].name);
					next = true;
					break;
				}
			}
			if (next)
				continue;
			/* handle bad variable */
			buffer = append_buffer(buffer, '$', &buffersize);
			buffer = append_buffer(buffer, *fmt, &buffersize);
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
	int nfmts = 0, totallen = 0, i, pos = 0;
	unsigned int fieldwidth, fieldlen, p;
	char *str = NULL, *tmp;
	fmt_field *this;
	bool free_tmp;

	/* determine field length */
	while (fmts[nfmts] != NULL)
	{
		if (fmts[nfmts]->width > 0)
			totallen += fmts[nfmts]->width;
		else
			totallen += 25;
		nfmts++;
	}

	/* build string */
	str = calloc(totallen, sizeof(char));
	for (i = 0; i < nfmts; i++)
	{
		this = fmts[i];
		tmp = NULL;

		/* convert field to string */
		tmp = field_to_str(this, &free_tmp, tsk);

		/* handle field string */
		if (tmp == NULL)
			continue;

		/* get string data */
		fieldlen = strlen(tmp);
		fieldwidth = this->width > 0 ? this->width : fieldlen;

		/* buffer right-aligned string */
		if (this->right_align)
		{
			for (p = 0; p < fieldwidth - fieldlen; p++)
				*(str + pos + p) = ' ';
			pos += fieldwidth - fieldlen;
		}

		/* copy string */
		strncpy(str+pos, tmp, MIN(fieldwidth, fieldlen));
		if (free_tmp)
			free(tmp);

		/* buffer left-aligned string */
		if (!(this->right_align))
		{
			for (p = fieldlen; p < fieldwidth; p++)
				*(str + pos + p) = ' ';
		}

		/* move current position */
		pos += this->right_align ? fieldwidth-fieldlen  : fieldwidth;
	}

	return str;
} /* }}} */

static char *field_to_str(fmt_field *this, bool *free_field, task *tsk) /* {{{ */
{
	/* evaluate a field and convert it to a string */
	char *ret = NULL;
	*free_field = true;

	switch(this->type)
	{
		case FIELD_STRING:
			ret = this->field;
			*free_field = false;
			break;
		case FIELD_DATE:
			ret = utc_date(0);
			break;
		case FIELD_VAR:
			ret = var_value_message(this->variable, false);
			break;
		case FIELD_PROJECT:
			ret = tsk->project;
			*free_field = false;
			break;
		case FIELD_DESCRIPTION:
			ret = tsk->description;
			*free_field = false;
			break;
		case FIELD_DUE:
			ret = utc_date(tsk->due);
			break;
		case FIELD_PRIORITY:
			if (tsk->priority)
			{
				ret = calloc(2, sizeof(char));
				*ret = tsk->priority;
			}
			break;
		case FIELD_UUID:
			ret = tsk->uuid;
			*free_field = false;
			break;
		case FIELD_INDEX:
			asprintf(&ret , "%d", tsk->index);
			break;
		default:
			break;
	}

	return ret;
} /* }}} */

conditional_fmt_field *parse_conditional(char *str, int *pos) /* {{{ */
{
	/* parse a conditional struct from a string @ position */
	conditional_fmt_field *this = calloc(1, sizeof(conditional_fmt_field));
	char *condition = NULL, *positive = NULL, *negative = NULL;
	int ret;

	/* parse fields */
	ret = sscanf(str+(*pos), "?%m[^?]?%m[^?]?%m[^?]?", &condition, &positive, &negative);
	if (ret != 3)
	{
		check_free(this);
		this = NULL;
		goto done;
	}

	/* compile each field */
	this->condition = compile_string(condition);
	this->positive = compile_string(positive);
	this->negative = compile_string(negative);

	goto done;

done:
	/* free allocated strings */
	check_free(condition);
	check_free(positive);
	check_free(negative);

	return this;
} /* }}} */
