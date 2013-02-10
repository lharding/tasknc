/**
 * @file common.h
 * @author mjheagle
 * @brief tasknc common macros
 */

#ifndef _TASKNC_COMMON_H
#define _TASKNC_COMMON_H

#define check_free(a) { if (a != NULL) free(a); }
#define check_fclose(a) { if (a != NULL) fclose(a); }

#endif
