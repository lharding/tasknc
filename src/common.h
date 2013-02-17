/**
 * @file common.h
 * @author mjheagle
 * @brief tasknc common macros
 */

#ifndef _TASKNC_COMMON_H
#define _TASKNC_COMMON_H

/* conditional free's */
#define check_free(a) { if (a != NULL) free(a); }
#define check_fclose(a) { if (a != NULL) fclose(a); }

/* variable definition */
enum var_type { VAR_UNKNOWN, VAR_STRING, VAR_INT, VAR_FLOAT, VAR_CHAR };
struct var {
        enum var_type type;
        union {
                char c;
                int d;
                float f;
                char *s;
        } value;
};

#endif
