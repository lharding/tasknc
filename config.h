/*
 * tasknc config
 * by mjheagle
 */

/* only import once */
#ifndef _TASKNC_CONFIG_H
#define _TASKNC_CONFIG_H

/* user defined field lengths */
#define TOTALLENGTH             1024
#define TAGSLENGTH              128
#define PROJECTLENGTH           64
#define DESCRIPTIONLENGTH       512
#define TIMELENGTH              32
#define LOGFILE                 "/tmp/.tasknc_runlog"

/* static field lengths */
#define UUIDLENGTH                      38
#define DATELENGTH                      7

/* compile in test suite */
#define TASKNC_INCLUDE_TESTS

/* enable macros */
#define ENABLE_MACROS

#endif

// vim: noet ts=4 sw=4 sts=4
