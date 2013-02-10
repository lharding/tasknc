/**
 * @file configure.h
 * @author mjheagle
 * @brief parse options for tasknc
 */

#ifndef _TASKNC_CONFIGURE_H
#define _TASKNC_CONFIGURE_H

/**
 * opaque representation of configuration struct
 * use the conf_get_* functions to access the struct's fields
 */
struct config;

/**
 * allocate a configuration struct and set default settings
 *
 * @return allocated configuration struct
 */
struct config *default_config();

/**
 * parse a configuration string
 *
 * @param conf configuration struct
 * @param str string to parse
 *
 * @return an indicator of success
 */
int config_parse_string(struct config *conf, char *str);

/**
 * parse a configuration file
 *
 * @param conf configuration struct
 * @param file file to parse
 *
 * @return an indicator of success
 */
int config_parse_file(struct config *conf, FILE *file);

/**
 * free a configuration struct
 *
 * @param conf struct to be free'd
 */
void free_config(struct config *conf);

#endif
