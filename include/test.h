/*
 * test.h
 * for tasknc
 * by mjheagle
 */

#ifndef _TEST_H
#define _TEST_H

#include "common.h"

void test(const char* args);

extern char* searchstring;
extern config cfg;
extern int selline;
extern struct task* head;

#endif

// vim: et ts=4 sw=4 sts=4
