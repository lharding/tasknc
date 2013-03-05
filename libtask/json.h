/**
 * @file json.h
 * @author mjheagle
 * @brief simple json parser for libtask
 */

#ifndef _TASK_JSON_H
#define _TASK_JSON_H

/**
 * parse a string containing json formatted text
 *
 * @param json string containing json formatted text
 *
 * @return an array of 'char *', containing an even number of elements.
 * elements are sequenced key, value in the array.
 */
char ** parse_json(const char *json);

#endif
