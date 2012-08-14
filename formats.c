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
extern config cfg;

/* local functions */
static char *append_buffer(char *, const char, int *);
static void append_field(fmt_field **, fmt_field **, fmt_field *);
static fmt_field *buffer_field(char *, int);
static char *eval_conditional(conditional_fmt_field *, task *);
static char *field_to_str(fmt_field *, bool *, task *);
static conditional_fmt_field *parse_conditional(char **);

char *append_buffer(char *buffer, const char append, int *bufferlen) /* {{{ */
{
	/* append a character to a buffer */
	(*bufferlen)++;
	buffer = realloc(buffer, ((*bufferlen)+1)*sizeof(char));
	buffer[*bufferlen-1] = append;
	buffer[*bufferlen] = 0;

	return buffer;
} /* }}} */

void append_field(fmt_field **head, fmt_field **last, fmt_field *this) /* {{{ */
{
	/* append a field to the field list */
	if (*head == NULL)
		*head = this;
	else
		(*last)->next = this;

	*last = this;
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

void compile_formats() /* {{{ */
{
	/* compile the format strings */
	cfg.formats.task_compiled = compile_string(cfg.formats.task);
	cfg.formats.title_compiled = compile_string(cfg.formats.title);
	cfg.formats.view_compiled = compile_string(cfg.formats.view);
} /* }}} */

fmt_field *compile_string(char *fmt) /* {{{ */
{
	/* compile a format string */
	fmt_field *head = NULL, *this, *last = NULL;
	int buffersize = 0, i, width;
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
		if (strchr("$?", *fmt) == NULL)
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
				append_field(&head, &last, this);
			}
			/* check for conditional */
			if (*fmt == '?')
			{
				this = calloc(1, sizeof(fmt_field));
				this->type = FIELD_CONDITIONAL;
				this->conditional = parse_conditional(&fmt);
				append_field(&head, &last, this);
				continue;
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
				append_field(&head, &last, this);
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
					append_field(&head, &last, this);
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
					append_field(&head, &last, this);
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
		append_field(&head, &last, this);
	}

	return head;
} /* }}} */

static char *eval_conditional(conditional_fmt_field *this, task *tsk) /* {{{ */
{
	/* evaluate a conditional struct to a string */
	char *tmp = NULL, *ret = NULL;

	/* handle empty conditionals */
	if (this == NULL)
	{
		ret = calloc(1, sizeof(char));
		*ret = 0;
		return ret;
	}

	/* evaluate conditional */
	tmp = eval_format(this->condition, tsk);

	/* check if conditional was "(null)" */
	if (str_eq(tmp, "(null)"))
		*tmp = 0;

	/* check first char of evaluated format at determine
	 * whether the positive or negative condition should
	 * be returned
	 */
	switch (*tmp)
	{
		case 0:
		case '0':
		case ' ':
			ret = eval_format(this->negative, tsk);
			break;
		default:
			ret = eval_format(this->positive, tsk);
			break;
	}
	free(tmp);

	return ret;
} /* }}} */

char *eval_format(fmt_field *fmts, task *tsk) /* {{{ */
{
	/* evaluate a format field array */
	int totallen = 0, pos = 0;
	unsigned int fieldwidth, fieldlen, p;
	char *str = NULL, *tmp;
	fmt_field *this;
	bool free_tmp;

	/* determine field length */
	for (this = fmts; this != NULL; this = this->next)
	{
		if (this->width > 0)
			totallen += this->width;
		else
			totallen += 100;
	}

	/* build string */
	str = calloc(totallen, sizeof(char));
	for (this = fmts; this != NULL; this = this->next)
	{
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
		case FIELD_CONDITIONAL:
			ret = eval_conditional(this->conditional, tsk);
			break;
		default:
			break;
	}

	return ret;
} /* }}} */

conditional_fmt_field *parse_conditional(char **str) /* {{{ */
{
	/* parse a conditional struct from a string @ position */
	conditional_fmt_field *this = calloc(1, sizeof(conditional_fmt_field));
	char *condition = NULL, *positive = NULL, *negative = NULL;
	int ret;

	/* parse fields */
	ret = sscanf(*str, "?%m[^?]?%m[^?]?%m[^?]?", &condition, &positive, &negative);
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

	/* move position of string */
	*str = ((*str) + 4 + strlen(condition) + strlen(positive) + strlen(negative));

	goto done;

done:
	/* free allocated strings */
	check_free(condition);
	check_free(positive);
	check_free(negative);

	return this;
} /* }}} */
