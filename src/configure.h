/**
 * @file configure.h
 * @author mjheagle
 * @brief parse options for tasknc
 */

#ifndef _TASKNC_CONFIGURE_H
#define _TASKNC_CONFIGURE_H

/**
 * opaque representation of configuration struct
 * use the conf_get_* and conf_set_* functions to access the struct's fields
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

/**
 * get log file descriptor
 *
 * @param conf configuration struct to obtain field from
 *
 * @return file descriptor for log file
 */
FILE *conf_get_logfd(struct config *conf);

/**
 * get task filter
 *
 * @param conf configuration struct to obtain field from
 *
 * @return string filter on tasks
 */
char *conf_get_filter(struct config *conf);

/**
 * set task filter
 *
 * @param conf configuration struct to set field in
 * @param filter the filter to set
 */
void conf_set_filter(struct config *conf, char *filter);

/**
 * get task sort
 *
 * @param conf configuration struct to obtain field from
 *
 * @return task sort order
 */
char *conf_get_sort(struct config *conf);

/**
 * set task sort
 *
 * @param conf configuration struct to set field in
 * @param sort the sort to set
 */
void conf_set_sort(struct config *conf, char *sort);

/**
 * get task task_format
 *
 * @param conf configuration struct to obtain field from
 *
 * @return task task format string
 */
char *conf_get_task_format(struct config *conf);

/**
 * set task task_format
 *
 * @param conf configuration struct to set field in
 * @param task_format task format string to apply
 */
void conf_set_task_format(struct config *conf, char *task_format);

/**
 * dump a config struct to a file
 *
 * @param out file to write to
 * @param conf configuration struct to dump
 */
void dump_config_file(FILE *out, struct config *conf);

#endif
