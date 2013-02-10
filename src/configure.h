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

/**
 * get nc_timeout from configuration struct
 *
 * @param conf configuration struct to obtain field from
 *
 * @return nc_timeout setting
 */
int conf_get_nc_timeout(struct config *conf);

/**
 * get task version from configuration struct
 *
 * @param conf configuration struct to obtain field from
 *
 * @return three digit array of integers
 */
int *conf_get_version(struct config *conf);

#endif
