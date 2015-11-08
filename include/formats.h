/*
 * formats.h
 * for tasknc
 * by mjheagle
 */

#ifndef _FIELDS_H
#define _FIELDS_H

#include "common.h"

struct fmt_field* compile_format_string(char* fmt);
char* eval_format(struct fmt_field* fmts, struct task* tsks);
void compile_formats(void);
void free_formats(void);

#endif

// vim: et ts=4 sw=4 sts=4
