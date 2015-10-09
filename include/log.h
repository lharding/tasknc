/*
 * log.h
 * for tasknc
 * by mjheagle
 */

#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include "common.h"

extern config cfg;
void tnc_fprintf(FILE* fp,
                 const enum log_mode monloglvl,
                 const char* format,
                 ...) __attribute__((format(printf, 3, 4)));

#endif

// vim: et ts=4 sw=4 sts=4
