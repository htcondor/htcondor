/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include <pwd.h>
#include <grp.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "safe.unix.h"
#include "parse_config.unix.h"

#define MAX_CONFIG_LINE		(8 * 1024)
#define MAX_CONFIG_VALUE_LENGTH	(1024 * 1024)
#define MAX_CONFIG_FILE_SIZE	(10 * 1024 * 1024)

int open_config_stream(config_file *cf, const char *filename, FILE * F)
{
    cf->line_num = 0;
    cf->bytes_read = 0;
    cf->prev_value_extra_lines = 0;
    cf->filename = filename;
    cf->F = F;

    return 0;
}

int open_config_file(config_file *cf, const char *filename)
{
    FILE *F = fopen(filename, "r");
    int r;

    if (F == NULL) {
        fatal_error_exit(1, "open_config_file(): fopen(%s) failed: %s",
                         filename, strerror(errno));
    }

    r = open_config_stream(cf, filename, F);

    return r;
}

int close_config_stream(config_file *cf)
{
    cf->F = 0;

    return 0;
}

int close_config_file(config_file *cf)
{
    int r = fclose(cf->F);
    if (r) {
        fatal_error_exit(1, "close_config_file(): fclose(%s): %s",
                         cf->filename, strerror(errno));
    }
    return close_config_stream(cf);
}

int next_cf_value(config_file *cf, char **key, char **value)
{
    char line_buf[MAX_CONFIG_LINE];
    char *sep_ptr;
    char *first_nonwhitespace_ptr;
    size_t line_len;

    while (1) {
        if (fgets(line_buf, sizeof line_buf, cf->F) == NULL) {
            if (feof(cf->F)) {
                return 0;
            }
        }

        if (cf->prev_value_extra_lines > 0) {
            cf->line_num += cf->prev_value_extra_lines;
            cf->prev_value_extra_lines = 0;
        }
        ++cf->line_num;

        line_len = strlen(line_buf);
        cf->bytes_read += line_len;

        if (cf->bytes_read > MAX_CONFIG_FILE_SIZE) {
            fatal_error_exit(1,
                             "configuration file is too large in file: %s",
                             cf->filename);
        }

        /* check for line too long */
        if (line_len == MAX_CONFIG_LINE - 1
            && line_buf[line_len - 1] != '\n') {
            fatal_error_exit(1,
                             "next_cf_value(): line too long in file: %s:%d",
                             cf->filename, cf->line_num);
        }

        /* remove trailing \n if it exists */
        if (line_buf[line_len - 1] == '\n') {
            line_buf[line_len - 1] = '\0';
            --line_len;
        }

        /* find first non-whitespace in line */
        first_nonwhitespace_ptr = line_buf;
        while (*first_nonwhitespace_ptr
               && isspace(*first_nonwhitespace_ptr)) {
            ++first_nonwhitespace_ptr;
        }

        /* if all whitespace or first non-whitespace is a comment char '#', get next line */
        if (!*first_nonwhitespace_ptr || *first_nonwhitespace_ptr == '#') {
            continue;
        }

        if ((sep_ptr = strchr(first_nonwhitespace_ptr, '='))) {
            /* line is of the form:    key = value */
            *key = trim_whitespace(first_nonwhitespace_ptr, sep_ptr);
            *value = trim_whitespace(sep_ptr + 1, line_buf + line_len);

        } else if ((sep_ptr = strchr(first_nonwhitespace_ptr, '<'))) {
            /* line is of the form:    key <n>
             *   where n bytes after the newline form the value
             */
            size_t len;
            char *endp;
            char *remaining_line;
            char *read_buf;
            size_t value_length;
            size_t num_read;

            *key = trim_whitespace(first_nonwhitespace_ptr, sep_ptr);
            errno = 0;
            len = strtoul(sep_ptr + 1, &endp, 10);

            if (errno != 0) {
                fatal_error_exit(1,
                                 "next_cf_value(): value length too large in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            if (sep_ptr + 1 == endp) {
                fatal_error_exit(1,
                                 "next_cf_value(): value length not numeric in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            remaining_line = trim_whitespace(endp, line_buf + line_len);
            if (remaining_line[0] != '>' || remaining_line[1] != '\0') {
                free(remaining_line);
                fatal_error_exit(1,
                                 "next_cf_value(): invalid line in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            free(remaining_line);
            remaining_line = 0;

            if (len > MAX_CONFIG_VALUE_LENGTH) {
                fatal_error_exit(1,
                                 "next_cf_value(): value length too large in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            /* malloc the buffer, read the data, check for errors */
            value_length = len;
            read_buf = (char *)malloc(value_length + 1);
            if (!read_buf) {
                fatal_error_exit(1,
                                 "next_cf_value(): malloc failed in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            num_read = fread(read_buf, value_length, 1, cf->F);

            if (num_read != 1) {
                fatal_error_exit(1,
                                 "next_cf_value(): fread read too little in file: %s:%d",
                                 cf->filename, cf->line_num);
            }

            read_buf[value_length] = '\0';

            *value = read_buf;

            /* count number of lines in the value to adjust the line number */
            sep_ptr = *value;
            while ((sep_ptr = strchr(sep_ptr, '\n'))) {
                ++cf->prev_value_extra_lines;
            }

            cf->bytes_read += value_length;
        } else {
            *key =
                trim_whitespace(first_nonwhitespace_ptr,
                                line_buf + line_len);
            *value = 0;
        }

        return 1;
    }
}
