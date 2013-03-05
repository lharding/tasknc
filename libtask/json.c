/*
 * simple json parser for libtask
 * by mjheagle
 */

#include <stdlib.h>
#include <string.h>

char ** parse_json(const char *json) {
        int counter = 0;

        /* check for valid start */
        if (*json != '{')
                return 0;

        /* create an array of 'char *' to store field/value pairs */
        int array_size = 12;
        char **ptrs = calloc(array_size, sizeof(char *));

        /* set head */
        char *pos = (char *)json;
        pos++;

        /* iterate through string */
        while (*pos != 0 && *pos != '}') {
                /* parse field */
                char *field_start = strchr(pos, '"');
                char *field_end = strchr(1+field_start, '"');
                const int field_len = field_end-field_start;
                char *field = calloc(field_len, sizeof(char));
                strncpy(field, field_start+1, field_len-1);

                /* parse value */
                char *value_start = field_end+2;
                char *value_end = strchr(value_start, ',');
                /* handle quoted value */
                if (*value_start == '"') {
                        while (value_end != NULL && (*(value_end-1) != '"' || *(value_end-2) == '\\'))
                                value_end = strchr(value_end+1, ',');
                }
                /* handle array */
                if (*value_start == '[') {
                        while (value_end != NULL && (*(value_end-1) != ']' || *(value_end-2) == '\\'))
                                value_end = strchr(value_end+1, ',');
                }
                /* handle end of line */
                if (value_end == NULL)
                        value_end = strchr(value_start, '}');
                /* remove start/end char from string if appropriate */
                if (*value_start == '"' || *value_start == '[') {
                        value_start++;
                        value_end--;
                }
                const int value_len = value_end-value_start+1;
                char *value = calloc(value_len, sizeof(char));
                strncpy(value, value_start, value_len-1);

                /* fix escaped characters */
                char *f = strchr(value, '\\');
                while (f != NULL) {
                        char *p;
                        for (p = f; *p != 0; p++)
                                *p = *(p+1);
                        f = strchr(f+1, '\\');
                }

                /* move head */
                pos = value_end + 1;
                if (*value_start == '"')
                        pos++;

                /* store pair */
                if (2*(counter+1) > array_size) {
                        array_size *= 2;
                        ptrs = realloc(ptrs, array_size*sizeof(char *));
                }
                ptrs[2*counter] = field;
                ptrs[2*counter+1] = value;

                counter++;
        }

        /* shrink ptrs array */
        if (counter) {
                ptrs = realloc(ptrs, 2*counter*sizeof(char *));
                ptrs[2*counter] = NULL;
        }

        return ptrs;
}
