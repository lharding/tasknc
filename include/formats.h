/*
 * formats.h
 * for tasknc
 * by mjheagle
 */

#ifndef _FIELDS_H
#define _FIELDS_H

#include "common.h"

fmt_field* compile_format_string(char* fmt);
char* eval_format(fmt_field* fmts, task* tsks);
void compile_formats(void);
void free_formats(void);

#endif

// vim: et ts=4 sw=4 sts=4
