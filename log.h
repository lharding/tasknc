/*
 * vim: noet ts=4 sw=4 sts=4
 *
 * log.h
 * for tasknc
 * by mjheagle
 */

#ifndef _LOG_H
#define _LOG_H

extern config cfg;
void tnc_fprintf(FILE *, const log_mode, const char *, ...) __attribute__((format(printf,3,4)));

#endif
