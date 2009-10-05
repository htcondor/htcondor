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
#include <dirent.h>
#include <signal.h>

#include <pwd.h>
#include <grp.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "config.h"

#if HAVE_SYS_PERSONALITY_H
# include <sys/personality.h>
#endif
#if HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#include "safe.h"

/***********************************************************************
 *
 * Initialize global variables
 *
 ***********************************************************************/

const id_t err_id = (id_t) -1;

/***********************************************************************
 *
 * Functions for handling fatal errors (writes to a stream and exits).
 *
 ***********************************************************************/

static FILE *err_stream = 0;

/*
 * fatal_error_exit
 *	This function tries to print an error message to the error stream
 *	setup by setup_err_stream or stderr if there is none.  It then does
 *	an immediate exit using _exit(status).
 * parameters
 * 	status
 *		exit status
 *	fmt, ...
 *		format and parameters as expected by printf
 * returns
 *	nothing
 */
void fatal_error_exit(int status, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (!err_stream) {
        err_stream = stderr;
    }

    vfprintf(err_stream, fmt, ap);
    fputc('\n', err_stream);

    /* could also call vsyslog here also */

    va_end(ap);

    fflush(err_stream);

    _exit(status);
}

/*
 * setup_err_stream
 *
 * parameters
 *	fd
 *		The file descriptor to use to send error messages when
 *		fatal_error_exit is called.  The file descriptor is dup'd
 *		and the new file descriptor is set to be close on exec.
 *		If any error is occurs in the dup, setting close on exec,
 *		or the fdopen fatal_error_exit is called.
 * returns
 *	nothing
 */
void setup_err_stream(int fd)
{
    int r;
    int flags;

    int new_fd = dup(fd);
    if (new_fd == -1) {
        fatal_error_exit(1, "dup failed on fd=%d: %s", fd,
                         strerror(errno));
    }

    flags = fcntl(new_fd, F_GETFD);
    if (flags == -1) {
        fatal_error_exit(1, "fcntl F_GETFD failed on fd=%d: %s", new_fd,
                         strerror(errno));
    }

    flags |= FD_CLOEXEC;
    r = fcntl(new_fd, F_SETFD, flags);
    if (flags == -1) {
        fatal_error_exit(1, "fcntl F_SETFD failed on fd=%d: %s", new_fd,
                         strerror(errno));
    }

    err_stream = fdopen(new_fd, "w");
    if (err_stream == NULL) {
        fatal_error_exit(1, "fdopen failed on fd=%d: %s", new_fd,
                         strerror(errno));
    }

    /* TODO: ask Jim if we can just not dup */
    close(fd);
}

/*
 * get_error_fd
 * 	Returns the file descriptor that was set by a call to setup_err_stream.
 * parameters
 * 	none
 * returns
 * 	The file descriptor or -1 if setup_err_stream had not been called.
 */
int get_error_fd()
{
    if (err_stream) {
        return fileno(err_stream);
    } else {
        return -1;
    }
}

/***********************************************************************
 *
 * Functions for manipulating string lists
 *
 ***********************************************************************/

/*
 * init_string_list
 * 	Initialize a string list structure.  Call fatal_error_exit if the malloc
 * 	fails
 * parameters
 * 	list
 * 		pointer to a string_list structure
 * returns
 * 	nothing
 */
void init_string_list(string_list *list)
{
    list->count = 0;
    list->capacity = 10;
    list->list = (char **) malloc(list->capacity * sizeof(list->list[0]));
    if (list->list == 0) {
        fatal_error_exit(1, "malloc failed in init_string_list");
    }
}

/*
 * add_one_capacity_to_string_list
 * 	Internal function to increase the capacity of the string_list if needed
 * 	to allow 1 additional element to be added to the current list.  Calls
 * 	fatal_erro_exit if malloc fails.
 * parameters
 * 	list
 * 		pointer to a string_list structure
 * returns
 * 	nothing
 */
static void add_one_capacity_to_string_list(string_list *list)
{
    if (list->count == list->capacity) {
        size_t new_capacity = 10 + 11 * list->capacity / 10;
        char **new_list =
            (char **) malloc(new_capacity * sizeof(new_list[0]));
        if (new_list == 0) {
            fatal_error_exit(1,
                             "malloc failed in add_one_capacity_to_string_list");
        }
        memcpy(new_list, list->list, list->count * sizeof(new_list[0]));
        free(list->list);
        list->list = new_list;
        list->capacity = new_capacity;
    }
}

/*
 * add_string_to_list
 * 	Add one string to the end of the string list
 * parameters
 * 	list
 * 		pointer to string list structure to add string
 * 	s
 * 		string to add to list
 * returns
 * 	nothing
 */
void add_string_to_list(string_list *list, char *s)
{
    add_one_capacity_to_string_list(list);
    list->list[list->count++] = s;
}

/*
 * prepend_string_to_list
 * 	Add one string to the beginning of a string list structure
 * parameters
 * 	list
 * 		pointer to string list structure to add string
 * 	s
 * 		string to add to list
 * returns
 * 	nothing
 */
void prepend_string_to_list(string_list *list, char *s)
{
    int i;

    add_one_capacity_to_string_list(list);
    for (i = list->count; i > 0; --i) {
        list->list[i] = list->list[i - 1];
    }
    ++list->count;
    list->list[0] = s;
}

/*
 * destroy_string_list
 * 	Destroy a string_list, including any memory acquired.
 * parameters
 * 	list
 * 		pointer to string list structure to destroy
 * returns
 * 	nothing
 */
void destroy_string_list(string_list *list)
{
    int i;
    for (i = 0; i < list->count; ++i) {
        free((char *) list->list[i]);
    }
    list->capacity = 0;
    list->count = 0;
    free(list->list);
    list->list = 0;
}

/*
 * null_terminated_list_from_string_list
 * 	Returns a pointer to an array of strings that is null terminatea,
 * 	and otherwise the same as the contents of the list.  The pointer
 * 	to the array is valid until a string_list is modified or destroyed.
 * parameters
 * 	list
 * 		pointer to string list structure to add string
 * returns
 * 	pointer to the null terminated array of strings
 */
char **null_terminated_list_from_string_list(string_list *list)
{
    add_one_capacity_to_string_list(list);
    list->list[list->count] = 0;
    return list->list;
}

/*
 * is_string_in_list
 * 	Returns true if the string exactly matches one of the entries in the
 * 	string list.
 * parameters
 * 	list
 * 		pointer to string list structure to add string
 * 	s
 * 		string to search for
 * returns
 * 	boolean of (string in the list)
 */
int is_string_in_list(string_list *list, const char *s)
{
    int i;
    for (i = 0; i < list->count; ++i) {
        if (!strcmp(list->list[i], s)) {
            return 1;
        }
    }

    return 0;
}

/*
 * is_string_list_empty
 * 	Returns true if the string_list contains 0 items.
 * parameters
 * 	list
 * 		pointer to string list structure to add string
 * returns
 * 	boolean of (string list is empty)
 */
int is_string_list_empty(string_list *list)
{
    return (list->count == 0);
}

/***********************************************************************
 *
 * Functions for manipulating id range lists
 *
 ***********************************************************************/

/*
 * init_id_range_list
 * 	Initialize an id_range_list structure.  Call fatal_error_exit if the
 * 	malloc fails.
 * parameters
 * 	list
 * 		pointer to an id range list
 * returns
 * 	nothing
 */
void init_id_range_list(id_range_list *list)
{
    list->count = 0;
    list->capacity = 10;
    list->list =
        (id_range_list_elem *) malloc(list->capacity *
                                      sizeof(list->list[0]));
    if (list->list == 0) {
        fatal_error_exit(1, "malloc failed in init_id_range_list");
    }
}

/*
 * add_id_range_to_list
 * 	Adds a range of ids (all the ids between min_id and max_id inclusive)
 * 	to the id_range_list.
 * parameters
 * 	list
 * 		pointer to an id range list
 * 	min_id
 * 		the minimum id of the range to add
 * 	max_id
 * 		the maximum id of the range to add
 * returns
 * 	nothing
 */
void add_id_range_to_list(id_range_list *list, id_t min_id, id_t max_id)
{
    if (list->count == list->capacity) {
        size_t new_capacity = 10 + 11 * list->capacity / 10;
        id_range_list_elem *new_list =
            (id_range_list_elem *) malloc(new_capacity *
                                          sizeof(new_list[0]));
        if (new_list == 0) {
            fatal_error_exit(1, "malloc failed in add_id_range_to_list");
        }
        memcpy(new_list, list->list, list->count * sizeof(new_list[0]));
        free(list->list);
        list->list = new_list;
        list->capacity = new_capacity;
    }

    list->list[list->count].min_value = min_id;
    list->list[list->count++].max_value = max_id;
}

/*
 * add_id_to_list
 * 	Add the single id to the list.  This is the same as calling
 * 	add_id_range_to_list with min_id and max_id set to id.
 * parameters
 * 	list
 * 		pointer to an id range list
 * 	id
 * 		the id to add
 * returns
 * 	nothing
 */
void add_id_to_list(id_range_list *list, id_t id)
{
    add_id_range_to_list(list, id, id);
}

/*
 * destroy_id_range_list
 * 	Destroy a id_range_list, including any memory have acquired.
 * parameters
 * 	list
 * 		pointer to id range list structure to destroy
 * returns
 * 	nothing
 */
void destroy_id_range_list(id_range_list *list)
{
    list->capacity = 0;
    list->count = 0;
    free(list->list);
    list->list = 0;
}

/*
 * is_id_in_list
 * 	Check if the id is in one of the id ranges in the id range list.
 * parameters
 * 	list
 * 		pointer to an id range list
 * 	id
 * 		the id to check
 * returns
 * 	True if the id is in one of the ranges, false otherwise.
 */
int is_id_in_list(id_range_list *list, id_t id)
{
    int i;
    for (i = 0; i < list->count; ++i) {
        if (list->list[i].min_value <= id && id <= list->list[i].max_value) {
            return 1;
        }
    }

    return 0;
}

/*
 * is_id_list_empty
 * 	Returns true if the id_range_list contains 0 ranges.
 * parameters
 * 	list
 * 		pointer to an id range list
 * returns
 * 	boolean of (id_range_list is empty)
 */
int is_id_list_empty(id_range_list *list)
{
    return (list->count == 0);
}

/***********************************************************************
 *
 * Functions for parsing ids, id ranges and id lists of numbers, uids and gids
 *
 ***********************************************************************/

/*
 * skip_whitespace_const
 * 	Returns a pointer to the first non-whitespace character in the
 * 	const string s.
 * parameters
 * 	s
 * 		the string to skip whitespace
 * returns
 * 	location of first non-whitespace
 */
const char *skip_whitespace_const(const char *s)
{
    while (*s && isspace(*s)) {
        ++s;
    }

    return s;
}

/*
 * skip_whitespace
 * 	Returns a pointer to the first non-whitespace character in the
 * 	string s.  Differs from skip_whitespace_const in only the const'ness
 * 	of the parameter and the returned pointer.
 * parameters
 * 	s
 * 		the string to skip whitespace
 * returns
 * 	location of first non-whitespace
 */
char *skip_whitespace(char *s)
{
    while (*s && isspace(*s)) {
        ++s;
    }

    return s;
}

/*
 * trim_whitespace
 * 	Returns a pointer to the first non-whitespace character in the
 * 	string s, and removes any trailing whitespace by modifying the
 * 	string by null-terminating it after the last non-whitespace.
 * parameters
 * 	s
 * 		the string to skip whitespace
 * returns
 * 	location of first non-whitespace
 */
char *trim_whitespace(const char *s, const char *endp)
{
    char *new_value;
    size_t trimmed_len;

    s = skip_whitespace_const(s);

    while (isspace(*(endp - 1)) && s < endp) {
        --endp;
    }

    trimmed_len = endp - s;
    new_value = (char *) malloc(trimmed_len + 1);
    if (new_value == 0) {
        fatal_error_exit(1, "malloc failed in trim_whitespace");
    }

    memcpy(new_value, s, trimmed_len);
    new_value[trimmed_len] = '\0';

    return new_value;
}

/*
 * name_to_error
 * 	Always return the err id (-1) and set errno to EINVAL.
 * parameters
 * 	name
 * 		unused
 * returns
 * 	err_id and errno = EINVAL
 */
static id_t name_to_error(const char *name)
{
    (void) name;
    errno = EINVAL;
    return err_id;
}

/*
 * name_to_uid
 * 	Return the uid matching the name if it exists.  Return the same
 * 	value as name_to_error if the name did not exist or there was an
 * 	error in getpwnam.
 * parameters
 * 	name
 * 		user name to lookup
 * returns
 * 	the uid corresponding to name if it exists, or the same as
 * 	name_to_error if it does not.
 */
static id_t name_to_uid(const char *name)
{
    struct passwd *pw = getpwnam(name);

    errno = 0;

    if (!pw) {
        return name_to_error(name);
    }

    return pw->pw_uid;
}

/*
 * name_to_gid
 * 	Return the gid matching the name if it exists.  Return the same
 * 	value as name_to_error if the name did not exist or there was an
 * 	error in getpwnam.
 * parameters
 * 	name
 * 		group name to lookup
 * returns
 * 	the gid corresponding to name if it exists, or the same as
 * 	name_to_error if it does not.
 */
static id_t name_to_gid(const char *name)
{
    struct group *gr = getgrnam(name);

    errno = 0;

    if (!gr) {
        return name_to_error(name);
    }

    return gr->gr_gid;
}

/*
 * strto_id
 * 	Return the id corresponding to the longest sequence of characters
 * 	that could form an unsigned integer or a name after skipping leading
 * 	whitespace.  An unsigned integer begins with 0-9 and continues until a
 * 	non-digit character.  A name starts with a non-digit character and
 * 	continues until a whitespace, end of string or a colon.
 *
 * 	The numeric form is converted to an unsigned integer and the name
 * 	form is converted to an unsigned integer using the lookup func.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	If there is nothing valid to try to convert to an id_t, then *endptr is
 * 	set to value.  This can only occur if value consists only of whitespace
 * 	characters.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	id
 * 		a pointer to store the id converted to a id_t
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * 	lookup
 * 		function pointer to convert a name to an id
 * returns
 * 	nothing, but sets *id, *endptr, and errno
 */
typedef id_t (*lookup_func) (const char *);

static void
strto_id(id_t *id, const char *value, const char **endptr,
         lookup_func lookup)
{
    const char *endp = value;

    const char *id_begin = skip_whitespace_const(value);

    errno = 0;

    if (isdigit(*id_begin)) {
        /* is numeric form, parse as a number */
        char *e;
        *id = strtoul(id_begin, &e, 10);
        endp = e;
    } else if (*id_begin) {
        /* is not numeric, parse as a name using lookup function */
        char *id_name;
        size_t id_len;

        /* find end - name can contain anything except whitespace and colons */
        endp = id_begin;
        while (*endp && !isspace(*endp) && *endp != ':') {
            ++endp;
        }

        id_len = endp - id_begin;
        id_name = malloc(id_len + 1);
        if (id_name == NULL) {
            fatal_error_exit(1, "malloc failed in strto_id");
        }
        memcpy(id_name, id_begin, id_len);
        id_name[id_len] = '\0';

        *id = lookup(id_name);
        free(id_name);
    } else {
        /* value contains nothing parsable */
        *id = err_id;
        errno = EINVAL;
    }

    if (endptr) {
        *endptr = endp;
    }
}

/*
 * parse_uid
 * 	Parse the string value and return the user id of the first id in the
 * 	string as a uid_t.  This follows the same rules as strto_id with
 * 	non-numeric ids converted to the matching user id.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	the user id, also updates *endptr and errno
 */
uid_t parse_uid(const char *value, const char **endptr)
{
    uid_t uid;

    strto_id(&uid, value, endptr, name_to_uid);

    return uid;
}

/*
 * parse_gid
 * 	Parse the string value and return the group id of the first id in the
 * 	string as gid_t.  This follows the same rules as strto_id with
 * 	non-numeric ids converted to the matching group id.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	the group id, also updates *endptr and errno
 */
gid_t parse_gid(const char *value, const char **endptr)
{
    gid_t gid;

    strto_id(&gid, value, endptr, name_to_gid);

    return gid;
}

/*
 * parse_id
 * 	Parse the string value and return the the first number in the string as
 * 	an id_t.  This follows the same rules as strto_id with non-numeric ids
 * 	returning an error.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	the group id, also updates *endptr and errno
 */
id_t parse_id(const char *value, const char **endptr)
{
    id_t id;

    strto_id(&id, value, endptr, name_to_error);

    return id;
}

/*
 * strto_id_range
 * 	Returns a pair of id's denoting a range of ids.  The form of the string
 * 	must be     <SP>* <id> [ <SP>* '-' <SP>* ( <id> | '*' ) ]?
 *
 * 	<id> is of the form parsed by strto_id.  If the option '-' and second
 * 	<id> is not present, the first <id> is returned for both the minimum
 * 	and maximum value.  Since an <id> in a non-numeric form may contain a
 * 	'-', a space must preceed the '-' if the first <id> is in a non-numeric
 * 	form.  The value '*' as the second value specifies the maximum allowed
 * 	id (assumes id_t is an unsigned type, if it is unsigned the code will
 * 	work correctly, but '*' will not work.
 *
 * 	It is an error if min_id is greater than max_id.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	min_id
 * 		a pointer to store the minimum id converted to a id_t
 * 	max_id
 * 		a pointer to store the maximum id converted to a id_t
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * 	lookup
 * 		function pointer to convert a name to an id
 * returns
 * 	nothing, but sets *min_id, *max_id, *endptr, and errno
 */
static void
strto_id_range(id_t *min_id, id_t *max_id, const char *value,
               const char **endptr, lookup_func lookup)
{
    const char *endp;
    strto_id(min_id, value, &endp, lookup);
    if (errno == 0 && value != endp) {
        /* parsed min correctly, check for a '-' and max value */
        value = skip_whitespace_const(endp);
        if (*value == '-') {
            ++value;
            value = skip_whitespace_const(value);
            if (*value == '*') {
                *max_id = UINT_MAX;
                endp = value + 1;
            } else {
                strto_id(max_id, value, &endp, lookup);
            }
        } else {
            *max_id = *min_id;
        }
    } else {
        *max_id = *min_id;
    }

    if (endptr) {
        *endptr = endp;
    }

    if (*min_id > *max_id) {
        errno = EINVAL;
    }
}

/*
 * strto_id_list
 * 	Adds the rnages in the value to the list.  Ranges are as specified in
 * 	strto_id_range, and there may be multiple ranges in value that are
 * 	separated by whitespace and a colon of the form:
 * 		<ID_RANGE> [ <SP>* ':' <SP>* <ID_RANGE> ]*
 *
 * 	Each range is of the form required by strto_id_range.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * 	lookup
 * 		function pointer to convert a name to an id
 * returns
 * 	nothing, but adds entries to *list, and sets *endptr, and errno
 */
static void
strto_id_list(id_range_list *list, const char *value, const char **endptr,
              lookup_func lookup)
{
    const char *endp = value;

    while (1) {
        id_t min_id;
        id_t max_id;

        strto_id_range(&min_id, &max_id, value, &endp, lookup);
        if (errno != 0 || value == endp) {
            break;
        }

        add_id_range_to_list(list, min_id, max_id);

        value = skip_whitespace_const(endp);
        if (*value == ':') {
            ++value;
        } else {
            break;
        }
    }

    if (endptr) {
        *endptr = endp;
    }
}

/*
 * parse_id_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are treated as errors.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	nothing, but adds entries to *list, and sets *endptr, and errno
 */
void
parse_id_list(id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_error);
}

/*
 * parse_uid_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are converted to ids by looking the
 * 	name as a username and returning its uid.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	nothing, but adds entries to *list, and sets *endptr, and errno
 */
void
parse_uid_list(id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_uid);
}

/*
 * parse_gid_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are converted to ids by looking the
 * 	name as a group name and returning its gid.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	If endptr is not NULL *endptr is set to point to the character one past
 * 	the end of the parsed value, just as strtoul() does.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * 	endptr
 * 		a pointer to one character past the end the sequence used to,
 * 		or NULL if the value is not needed
 * returns
 * 	nothing, but adds entries to *list, and sets *endptr, and errno
 */
void
parse_gid_list(id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_gid);
}

/***********************************************************************
 *
 * Environment related functions
 *
 ***********************************************************************/

#if !HAVE_SETENV
/*
 * setenv
 *   Implementation of setenv for platforms that don't have it. Leaks
 *   memory if called twice for the same variable, but it should be
 *   sufficient for (a) our simple use of setenv from this module (b)
 *   on the one or two platforms we have that don't have setenv.
 */
static int setenv(const char *var, const char *val, int overwrite)
{
    char *tmp;

    (void) overwrite;

    tmp = malloc(strlen(var) + strlen(val) + 2);
    if (tmp == NULL) {
        return -1;
    }
    sprintf(tmp, "%s=%s", var, val);
    if (putenv(tmp) != 0) {
        free(tmp);
        return -1;
    }
    return 0;
}
#endif

#if !HAVE_CLEARENV
/*
 * clearenv
 * 	Clears the environment.  There is no standard for clearing the
 * 	environment, nor is there a way to do it portably if you later
 * 	wish to setenv, getenv, and unsetenv, as you are not allowed
 * 	to use them after directly changing the environ variable
 *
 * 	This function should work in most cases and on many platforms
 * 	is part of the standard library.
 * parameters
 * 	n/a
 * returns
 * 	0 on success.  non-zero on failure.
 */
extern char **environ;
int clearenv(void)
{
    int r = 0;                      /* status value */
    const char var_name[] = "x";    /* temp env name to set and delete */

    /* setting environ will make most implementation of getenv, setenv,
     * unsetenv, and putenv properly work and recreate the environment
     * if needed */
    environ = 0;

#if HAVE_UNSETENV
    /* set an environment and clear it so the environment array gets
     * allocated properly and will work correctly if code later tries
     * to access directly */
    r = setenv(var_name, "", 0);
    if (r == 0) {
        unsetenv(var_name);
    }
#endif

    return r;
}
#endif

/*
 * safe_reset_environment
 * 	Clears the environment, and retains the TZ value and sets
 * 	the PATH to a minimal reasonable value. This will leak
 * 	memory (from the strdup of the TZ variable) if called
 * 	multiple times.
 * parameters
 * 	n/a
 * returns
 * 	0 on success, -1 on failure.
 */
int safe_reset_environment()
{
    int r;                      /* status value */

    const char tz_name[] = "TZ";
    const char path_name[] = "PATH";
    const char path_value[] = "/bin:/usr/bin";

    /* get and save the TZ value if it exists */
    char *tz = getenv(tz_name);
    if (tz) {
        tz = strdup(tz);
        if (tz == NULL) {
            return -1;
        }
    }

    /* clear the environment */
    r = clearenv();
    if (r != 0) {
        return -1;
    }

    /* set PATH and TZ */
    r = setenv(path_name, path_value, 1);
    if (r == -1) {
        return -1;
    }

    if (tz) {
        r = setenv(tz_name, tz, 1);
        if (r == -1) {
            return -1;
        }

        free(tz);
    }

    return 0;
}

/***********************************************************************
 *
 * File descriptor related functions
 *
 ***********************************************************************/

/*
 * get_open_max
 *	Return the number of open file descriptors that a process can have open
 * parameters
 * 	n/a
 * returns
 * 	maximum number of file descriptors that can be open
 */
static int get_open_max(void)
{
    static int open_max = 0;

#ifdef OPEN_MAX
    open_max = OPEN_MAX;
#endif

    if (!open_max) {
        open_max = sysconf(_SC_OPEN_MAX);
        if (open_max < 0) {
            struct rlimit rlim;
            int r = getrlimit(RLIMIT_NOFILE, &rlim);
            if (r == 0 && rlim.rlim_max != RLIM_INFINITY) {
                open_max = rlim.rlim_max;
            } else {
                /* could not find the max */
                open_max = -1;
            }
        }
    }

    return open_max;
}

/*
 * safe_close_fds_between
 *	Close all file descriptors that are >= min_fd and <= max_fd
 * parameters
 * 	min_fd
 * 		minimum file descriptor to close
 * 	max_fd
 * 		maximum file descriptor to close
 * returns
 * 	0 on success, the result of the failing close on failure
 */
int safe_close_fds_between(int min_fd, int max_fd)
{
    int fd;

    for (fd = min_fd; fd < max_fd; ++fd) {
        int r = close(fd);
        /* EBADF means the descriptor was not open, which is ok */
        if (r == -1 && errno != EBADF) {
            return r;
        }
    }

    return 0;
}

/*
 * safe_close_fds_starting_at
 *	Close all file descriptors that are >= min_fd
 * parameters
 * 	min_fd
 * 		minimum file descriptor to begin closing
 * returns
 * 	0 on success, non-zero on failure
 */
int safe_close_fds_starting_at(int min_fd)
{
    int open_max = get_open_max();
    if (open_max == -1) {
        /* could not find the max, panic! */
        return -1;
    }

    return safe_close_fds_between(min_fd, open_max - 1);
}

/*
 * safe_close_fds_except
 *	Close all file descriptors except those in list
 * parameters
 * 	ids
 * 		list of file descriptors to keep open
 * returns
 * 	0 on success, non-zero on failure
 */
int safe_close_fds_except(id_range_list *ids)
{
    int fd;
    int open_max = get_open_max();
    if (open_max == -1) {
        /* could not find the max, panic! */
        return -1;
    }

    for (fd = 0; fd < open_max; ++fd) {
        if (!is_id_in_list(ids, fd)) {
            int r = close(fd);
            /* EBADF means the descriptor was not open, which is ok */
            if (r == -1 && errno != EBADF) {
                return r;
            }
        }
    }

    return 0;
}

const char DEV_NULL_FILENAME[] = "/dev/null";

/*
 * safe_open_std_file
 *	Opens the standard file descriptors for standard in, out and err
 *	to filename with the appropriate mode.  If filename is NULL
 *	the descriptor is opened to /dev/null.  If the file is created
 *	the permissions are such at only the owner can read or write its
 *	contents.
 * parameters
 * 	fd
 * 		the number of the file descriptor to open
 * 	filename
 * 		the filename to open;  if NULL use "/dev/null"
 * returns
 * 	0 on success, non-zero on failure
 */
int safe_open_std_file(int fd, const char *filename)
{
    int new_fd = -1;
    const int open_mask = S_IRUSR | S_IWUSR;    /* read & write for user only */

    if (!filename) {
        filename = DEV_NULL_FILENAME;
    }

    if (fd < 0 || fd > 2) {
        errno = EINVAL;
        return -1;
    }

    (void) close(fd);

    /* open the files */
    if (fd == 0) {
        new_fd = open(filename, O_RDONLY);
    } else if (fd == 1 || fd == 2) {
        /* set umask so it doesn't restrict permissions we want */
        int old_mask = umask(0);
        new_fd = open(filename, O_CREAT | O_EXCL | O_WRONLY, open_mask);
        (void) umask(old_mask);
        if (new_fd == -1 && errno == EEXIST) {
            new_fd = open(filename, O_WRONLY);
        }
    }

    if (new_fd == -1) {
        return -1;
    }

    /* if new_fd is not the right value, dup2 it to the right value */
    if (new_fd != fd) {
        int r = dup2(new_fd, fd);
        if (r == -1) {
            (void) close(new_fd);
            return -1;
        }

        (void) close(new_fd);
    }

    return 0;
}

/*
 * safe_open_std_files_to_null
 *	Opens file descriptors 0, 1 and 2 to /dev/null.
 * parameters
 * 	n/a
 * returns
 * 	0 on success, non-zero on failure
 */
int safe_open_std_files_to_null()
{
    int i;

    for (i = 0; i <= 2; ++i) {
        int r = safe_open_std_file(i, 0);
        if (r == -1) {
            return -1;
        }
    }

    return 0;
}

/***********************************************************************
 *
 * Function to safely switch user and groups
 *
 ***********************************************************************/

/*
 * safe_checks_and_set_gids
 *	Checks that the uid exists in the password file and is in the list of
 *	safe_uids.  Checks that the primary group and the supplementary groups
 *	as determined from the password and group files are in the safe_gids
 *	list.
 *
 *	As a side effect returns the primary group associated with the uid
 *	and sets the supplementary groups for uid.
 * parameters
 * 	uid
 * 		the uid to test and to get group information
 * 	tracking_gid
 * 		if nonzero, an additional GID to place in the supplementary
 * 		groups list for tracking purposes
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * 	primary_gid
 * 		a pointer to gid_t in which the primary group is stored on
 * 		success
 * returns
 * 	0 on success, non-zero on failure
 * 	-1	uid not safe
 * 	-2	uid not in password file
 * 	-3	primary group not safe
 * 	-4	couldn't set supplementary groups
 * 	-5	couldn't get number of supplementary groups
 * 	-6	malloc failed
 * 	-7	couldn't get supplementary groups
 * 	-8	a supplementary group was not safe
 * 	-12	the tracking GID could not be inserted
 */
static int
safe_checks_and_set_gids(uid_t uid,
                         gid_t tracking_gid,
                         id_range_list *safe_uids,
                         id_range_list *safe_gids,
                         gid_t * primary_gid)
{
    struct passwd *pw;
    gid_t gid;
    int num_groups;
    int total_groups;
    gid_t *gids;
    int r;
    int i;

    /* check uid is in safe_uids */
    if (!is_id_in_list(safe_uids, uid)) {
        return -1;
    }

    /* check tracking GID, if given, is safe */
    if ((tracking_gid != 0) && !is_id_in_list(safe_gids, tracking_gid)) {
		return -8;
	}

    /* check uid is in the password file */
    pw = getpwuid(uid);
    if (pw == NULL) {
        return -2;
    }

    /* get primary group */
    gid = pw->pw_gid;

    /* check primary group is in safe_gids */
    if (!is_id_in_list(safe_gids, gid)) {
        return -3;
    }

    /* set supplementary groups */
    r = initgroups(pw->pw_name, gid);
    if (r == -1) {
        return -4;
    }

    /* get number of supplementary groups */
    total_groups = num_groups = getgroups(0, 0);
    if (num_groups == -1) {
        return -5;
    }
    if (tracking_gid != 0) {
        total_groups++;
    }
    gids = (gid_t *) malloc(total_groups * sizeof(gid_t));
    if (gids == NULL) {
        return -6;
    }

    /* get all supplementary groups */
    num_groups = getgroups(num_groups, gids);
    if (num_groups == -1) {
        free(gids);
        return -7;
    }

    /* verify that all supplementary groups are in safe_gids */
    for (i = 0; i < num_groups; ++i) {
        if (!is_id_in_list(safe_gids, gids[i])) {
            free(gids);
            return -8;
        }
    }

    /* add in the tracking GID if given */
    if (tracking_gid != 0) {
        gids[num_groups] = tracking_gid;
        if (setgroups(num_groups + 1, gids) == -1) {
            free(gids);
            return -12;
        }
    }

    free(gids);

    /* return primary group */
    *primary_gid = gid;

    return 0;
}

/*
 * safe_switch_to_uid
 *	Set the real uid, real gid and supplementary groups as appropriate for
 *	the uid.  This does not drop the saved user and group ids.
 * parameters
 * 	uid
 * 		the uid to test and to get group information
 * 	tracking_gid
 * 		if nonzero, an additional GID that will be put in the supplementary
 * 		group list for tracking
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 * 	0 on success, non-zero on failure
 * 	same status as safe_check_and_set_gids plus:
 * 	-9	couldn't set real gid
 * 	-10	couldn't set real uid
 * 	-11	some id did not change properly
 * 	-12	couldn't insert tracking GID
 */
int
safe_switch_to_uid(uid_t uid,
                   gid_t tracking_gid,
                   id_range_list *safe_uids,
                   id_range_list *safe_gids)
{
    gid_t gid;
    int r;

    r = safe_checks_and_set_gids(uid,
                                 tracking_gid,
                                 safe_uids,
                                 safe_gids,
                                 &gid);
    if (r != 0) {
        return r;
    }

    r = setgid(gid);
    if (r == -1) {
        return -9;
    }

    r = setuid(uid);
    if (r == -1) {
        return -10;
    }

    /* check real and effective user ids were changed to uid
     * and the real and effective group ids were changed to gid */
    if (getuid() != uid || geteuid() != uid || getgid() != gid
        || getegid() != gid) {
        return -11;
    }

    return 0;
}

/*
 * safe_switch_effective_to_uid
 *	Set the effective uid, real gid and supplementary groups as appropriate
 *	for the uid.  This does not drop the saved user and group ids.
 * parameters
 * 	uid
 * 		the uid to test and to get group information
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 * 	0 on success, non-zero on failure
 * 	same status as safe_check_and_set_gids plus:
 * 	-9	couldn't set effective gid
 * 	-10	couldn't set effective uid
 */
int
safe_switch_effective_to_uid(uid_t uid, id_range_list *safe_uids,
                             id_range_list *safe_gids)
{
    gid_t gid;
    int r;

    r = safe_checks_and_set_gids(uid, 0, safe_uids, safe_gids, &gid);
    if (r != 0) {
        return r;
    }

    r = setegid(gid);
    if (r == -1) {
        return -9;
    }

    r = seteuid(uid);
    if (r == -1) {
        return -10;
    }

    return 0;
}

/***********************************************************************
 *
 * Safely exec'ing a program as a user
 *
 ***********************************************************************/

/*
 * safe_exec_as_user
 * 	Change to a user and exec a program.  The user id, group id and
 * 	supplementary group ids are changed based on the supplied uid.
 * 	The new user and group ids must be in the list of safe ids.
 *
 * 	All open file descriptors except those in the keep_open_fds
 * 	list are closed
 *
 * 	The initial directory is set to the initial_dir after switching
 * 	to ids, so the uid must have access to the directory. 
 *
 * 	Standard in, out and err are opened to the filename specified
 * 	(a NULL value will open the /dev/null file), unless the file
 * 	descriptor number is in the keep_open_fds list.  This is done
 * 	after the switch of ids, so the files must be accessible by
 * 	the uid given.
 *
 * 	After the process parameters are set, execve is called with exec_name,
 * 	args and env.
 * parameters
 * 	uid
 * 		the uid to test and to get group information
 * 	tracking_gid
 * 		if nonzero, an additional GID that will be placed into the
 * 		the process's supplementary group list for tracking purposes
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * 	exec_name
 * 		the complete path to the executable to start
 * 	args
 * 		the arguments to the process as a null terminated string list
 * 		as of the form expected by execve (args[0] by convention should
 * 		be the same as exec_name)
 * 	env
 * 		the initial environment for the process as a null terminated
 * 		string list of the form expected by execve.
 * 	keep_open_fds
 * 		the list of file descriptors not to close taking precedence
 * 		over stdin_filename, stdout_filename and stderr_filename
 * 	stdin_filename
 * 		the name of the file to open for standard in.  A NULL
 * 		indicates no file (/dev/null).  if 0 is in keep_open_fds
 * 		this is ignored.
 * 	stdout_filename
 * 		the name of the file to open for standard out.  A NULL
 * 		indicates no file (/dev/null).  if 0 is in keep_open_fds
 * 		this is ignored.
 * 	stderr_filename
 * 		the name of the file to open for standard err.  A NULL
 * 		indicates no file (/dev/null).  if 0 is in keep_open_fds
 * 		this is ignored.
 * 	initial_dir
 * 		the initial working directory for the executable.  A NULL
 * 		will cause the root directory to be used ("/").
 * 	is_std_univ
 * 		triggers special logic to prepare the process so that it
 * 		can be run under Condor's standard universe
 * returns
 * 	does not return on success, non-zero on failure
 */
int
safe_exec_as_user(uid_t uid,
                  gid_t tracking_gid,
                  id_range_list *safe_uids,
                  id_range_list *safe_gids,
                  const char *exec_name,
                  char **args,
                  char **env,
                  id_range_list *keep_open_fds,
                  const char *stdin_filename,
                  const char *stdout_filename,
                  const char *stderr_filename,
                  const char *initial_dir,
                  int is_std_univ)
{
    int r;

    /* switch real and effective user and groups ids along with supplementary
     * groups, verify that they are all safe */
    r = safe_switch_to_uid(uid, tracking_gid, safe_uids, safe_gids);
    if (r < 0) {
        fatal_error_exit(1, "error switching to uid %lu",
                         (unsigned long) uid);
    }

    /* change the directory to initial_dir or "/" if NULL */
    if (!initial_dir) {
        initial_dir = "/";
    }

    r = chdir(initial_dir);
    if (r == -1) {
        fatal_error_exit(1, "error chdir to (%s)", initial_dir);
    }

    /* close all file descriptors except these */
    r = safe_close_fds_except(keep_open_fds);
    if (r == -1) {
        fatal_error_exit(1, "error closing all file descriptors",
                         initial_dir);
    }

    /* open standard file descriptors to those specified or /dev/null if NULL,
     * unless they are in keep_open_fds */
    if (!is_id_in_list(keep_open_fds, 0)) {
        r = safe_open_std_file(0, stdin_filename);
        if (r == -1) {
            fatal_error_exit(1, "error opening stdin");
        }
    }

    if (!is_id_in_list(keep_open_fds, 1)) {
        r = safe_open_std_file(1, stdout_filename);
        if (r == -1) {
            fatal_error_exit(1, "error opening stdout");
        }
    }

    if (!is_id_in_list(keep_open_fds, 2)) {
        r = safe_open_std_file(2, stderr_filename);
        if (r == -1) {
            fatal_error_exit(1, "error opening stderr");
        }
    }
#if defined(LINUX) && (defined(I386) || defined(X86_64))
    /*
       on linux, set the personality for std univ jobs
       - PER_LINUX32 turns off exec shield
       - 0x4000 is ADDR_NO_RANDOMIZE, but the macro is
       not defined in many of our supported distros
     */
    if (is_std_univ) {
#if defined(I386)
        unsigned long persona = PER_LINUX32 | 0x40000;
#elif defined(X86_64)
        unsigned long persona = 0x40000;
#endif
        if (syscall(SYS_personality, persona) == -1) {
            fatal_error_exit(1, "error setting personality: %s");
        }
    }
#endif

    /* finally do the exec */
    r = execve(exec_name, (char **) args, (char **) env);
    if (r == -1) {
        fatal_error_exit(1, "error exec'ing: %s", strerror(errno));
    }

    return 0;
}

/***********************************************************************
 *
 * Functions to check the safety of the directory or path
 *
 ***********************************************************************/

/*
 * is_mode_trusted
 * 	Returns trustedness of mode
 * parameters
 * 	stat_buf
 * 		the result of the stat system call on the entry in question
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
static int
is_mode_trusted(struct stat *stat_buf, id_range_list *trusted_uids,
                id_range_list *trusted_gids)
{
    mode_t mode = stat_buf->st_mode;
    uid_t uid = stat_buf->st_uid;
    gid_t gid = stat_buf->st_gid;
    int is_untrusted_uid = (uid != 0 && !is_id_in_list(trusted_uids, uid));
    int is_dir = S_ISDIR(mode);
    int is_untrusted_group = !is_id_in_list(trusted_gids, gid);
    int is_untrusted_group_writable = is_untrusted_group
        && (mode & S_IWGRP);
    int is_other_writable = (mode & S_IWOTH);

    int is_trusted = PATH_UNTRUSTED;

    if (!
        (is_untrusted_uid || is_untrusted_group_writable
         || is_other_writable)) {
        int oth_read_mask = is_dir ? S_IXOTH : S_IROTH;
        int is_other_readable = (mode & oth_read_mask);

        int grp_read_mask = is_dir ? S_IXGRP : S_IRGRP;
        int is_untrusted_group_readable = is_untrusted_group
            && (mode & grp_read_mask);

        if (is_other_readable || is_untrusted_group_readable) {
            is_trusted = PATH_TRUSTED;
        } else {
            is_trusted = PATH_TRUSTED_CONFIDENTIAL;
        }
    } else if (S_ISLNK(mode)) {
        is_trusted = PATH_TRUSTED;
    } else {
        int is_sticky_dir = (mode & S_ISVTX) && is_dir;

        if (is_sticky_dir && !is_untrusted_uid) {
            is_trusted = PATH_TRUSTED_STICKY_DIR;
        }
    }

    return is_trusted;
}

/* abbreviations to make trust_matrix initialization easier to read */
enum { PATH_U = PATH_UNTRUSTED,
    PATH_S = PATH_TRUSTED_STICKY_DIR,
    PATH_T = PATH_TRUSTED,
    PATH_C = PATH_TRUSTED_CONFIDENTIAL
};

/* trust composition table, given the trust of the parent directory and the child
 * this is only valid for directories.  is_component_in_dir_trusted() modifies it
 * slightly for other file system types
 */
static int trust_matrix[][4] = {
    /*      parent\child |  PATH_U  PATH_U  PATH_U  PATH_U  */
    /*      ------          ------------------------------  */
    /*      PATH_U  */ {PATH_U, PATH_U, PATH_U, PATH_U},
    /*      PATH_S  */ {PATH_U, PATH_S, PATH_T, PATH_C},
    /*      PATH_T  */ {PATH_U, PATH_S, PATH_T, PATH_C},
    /*      PATH_C  */ {PATH_U, PATH_S, PATH_T, PATH_C}
};

/*
 * is_component_in_dir_trusted
 * 	Returns trustedness of mode.  See trust_matrix above, plus if the
 * 	parent directory is a stick bit directory everything that can be
 * 	hard linked (everyting except directories) is PATH_UNTRUSTED.
 * parameters
 * 	parent_dir_trust
 * 		trust level of parent directory
 * 	child_stat_buf
 * 		the result of the stat system call on the entry in question
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
static int
is_component_in_dir_trusted(int parent_dir_trust,
                            struct stat *child_stat_buf,
                            id_range_list *trusted_uids,
                            id_range_list *trusted_gids)
{
    int child_trust =
        is_mode_trusted(child_stat_buf, trusted_uids, trusted_gids);

    int status = trust_matrix[parent_dir_trust][child_trust];

    int is_dir = S_ISDIR(child_stat_buf->st_mode);
    if (parent_dir_trust == PATH_TRUSTED_STICKY_DIR && !is_dir) {
        /* anything in a sticky bit directory is untrusted, except a directory */
        status = PATH_UNTRUSTED;
    }

    return status;
}

/*
 * is_current_working_directory_trusted
 * 	Returns the trustedness of the current working directory.  If any
 * 	directory from here to the root is untrusted the path is untrusted,
 * 	otherwise it returns the trustedness of the current working directory.
 *
 * 	This function is not thread safe if other threads depend on the value
 * 	of the current working directory as it changes the current working
 * 	directory while checking the path and restores it on exit.
 * parameters
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
static int
is_current_working_directory_trusted(id_range_list *trusted_uids,
                                     id_range_list *trusted_gids)
{
    int saved_dir = -1;
    int parent_dir_fd = -1;
    int r;
    int status = PATH_UNTRUSTED;        /* trust of cwd or error value */
    int cur_status;             /* trust of current directory being checked */
    struct stat cur_stat;
    struct stat prev_stat;
    int not_at_root;

    /* save the cwd, so it can be restored */
    saved_dir = open(".", O_RDONLY);
    if (saved_dir == -1) {
        status = -1;
        goto restore_dir_and_exit;
    }

    r = fstat(saved_dir, &cur_stat);
    if (r == -1) {
        status = -4;
        goto restore_dir_and_exit;
    }

    /* Walk the directory tree, from the directory given to the root.
     *
     * If there is a directory that is_trusted_mode returns PATH_UNTRUSTED
     * exit immediately with that value
     *
     * Assumes no hard links to directories.
     */
    do {
        cur_status =
            is_mode_trusted(&cur_stat, trusted_uids, trusted_gids);
        if (status == PATH_UNTRUSTED) {
            /* this is true only the first time through the loop (the cwd).
             * The return result is the value of the cwd.
             */
            status = cur_status;
        }

        if (cur_status == PATH_UNTRUSTED) {
            /* untrusted directory persmissions */
            status = PATH_UNTRUSTED;
            goto restore_dir_and_exit;
        }

        prev_stat = cur_stat;

        /* get handle to parent directory */
        parent_dir_fd = open("..", O_RDONLY);
        if (parent_dir_fd == -1) {
            status = -6;
            goto restore_dir_and_exit;
        }

        /* get the parent directory stat buffer */
        r = fstat(parent_dir_fd, &cur_stat);
        if (r == -1) {
            status = -7;
            goto restore_dir_and_exit;
        }

        /* check if we are at the root directory */
        not_at_root = cur_stat.st_dev != prev_stat.st_dev
            || cur_stat.st_ino != prev_stat.st_ino;

        if (not_at_root) {
            /* not at root, change directory to parent */
            r = fchdir(parent_dir_fd);
            if (r == -1) {
                status = -8;
                goto restore_dir_and_exit;
            }
        }

        /* done with parent directory handle, close it */
        r = close(parent_dir_fd);
        if (r == -1) {
            status = -9;
            goto restore_dir_and_exit;
        }

        parent_dir_fd = -1;
    } while (not_at_root);

  restore_dir_and_exit:
    /* restore the old working directory & close open file descriptors if needed
     * and return value 
     */

    if (saved_dir != -1) {
        r = fchdir(saved_dir);
        if (r == -1) {
            status += -100;
        }
        r = close(saved_dir);
        if (r == -1) {
            status += -200;
        }
    }
    if (parent_dir_fd != -1) {
        r = close(parent_dir_fd);
        if (r == -1) {
            status += -400;
        }
    }

    return status;
}

/*
 * is_current_working_directory_trusted_r
 * 	Returns the trustedness of the current working directory.  If any
 * 	directory from here to the root is untrusted the path is untrusted,
 * 	otherwise it returns the trustedness of the current working directory.
 * parameters
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
static int
is_current_working_directory_trusted_r(id_range_list *trusted_uids,
                                       id_range_list *trusted_gids)
{
    int r;
    int status = PATH_UNTRUSTED;        /* trust of cwd or error value */
    int cur_status;             /* trust of current directory being checked */
    struct stat cur_stat;
    struct stat prev_stat;
    int not_at_root;
    char path[PATH_MAX] = ".";
    char *path_end = &path[0];

    r = lstat(path, &cur_stat);
    if (r == -1) {
        return -1;
    }

    /* Walk the directory tree, from the directory given to the root.
     *
     * If there is a directory that is_trusted_mode returns PATH_UNTRUSTED
     * exit immediately with that value
     *
     * Assumes no hard links to directories.
     */
    do {
        cur_status =
            is_mode_trusted(&cur_stat, trusted_uids, trusted_gids);
        if (status == PATH_UNTRUSTED) {
            /* this is true only the first time through the loop (the cwd).
             * The return result is the value of the cwd.
             */
            status = cur_status;
        }

        if (cur_status == PATH_UNTRUSTED) {
            /* untrusted directory persmissions */
            return PATH_UNTRUSTED;
        }

        prev_stat = cur_stat;

        if (path_end != path) {
            /* if not the first time through, append a directory separator */
            if ((size_t) (path_end - path + 1) > sizeof(path)) {
                errno = ENAMETOOLONG;
                return -1;
            }

            *path_end++ = '/';
            *path_end = '\0';
        }

        /* append a parent directory, .. */
        if ((size_t) (path_end - path + 1) > sizeof(path)) {
            errno = ENAMETOOLONG;
            return -1;
        }

        *path_end++ = '.';
        *path_end++ = '.';
        *path_end = '\0';

        /* get the parent directory stat buffer */
        r = lstat(path, &cur_stat);
        if (r == -1) {
            return -1;
        }

        /* check if we are at the root directory */
        not_at_root = cur_stat.st_dev != prev_stat.st_dev
            || cur_stat.st_ino != prev_stat.st_ino;
    } while (not_at_root);

    return status;
}

#ifdef SYMLOOP_MAX
#define MAX_SYMLINK_DEPTH SYMLOOP_MAX
#else
#define MAX_SYMLINK_DEPTH 32
#endif

typedef struct dir_stack {
    struct dir_path {
        char *original_ptr;
        char *cur_position;
    } stack[MAX_SYMLINK_DEPTH];

    int count;
} dir_stack;

/*
 * init_dir_stack
 * 	Initialize a dir_stack data structure
 * parameters
 * 	stack
 * 		pointer to a dir_stack to initialize
 * returns
 *	Nothing
 */
static void init_dir_stack(dir_stack *stack)
{
    stack->count = 0;
}

/*
 * destroy_dir_stack
 * 	Destroy a dir_stack data structure, free's unfreed paths that have
 * 	been pushed onto the stack
 * parameters
 * 	stack
 * 		pointer to a dir_stack to destroy
 * returns
 *	Nothing
 */
static void destroy_dir_stack(dir_stack *stack)
{
    while (stack->count > 0) {
        free(stack->stack[--stack->count].original_ptr);
    }
}

/*
 * push_path_on_stack
 * 	Pushes a copy of the path onto the directory stack
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get pushed
 *	path
 *		path to push on the stack.  A copy is made.
 * returns
 *	0  on sucess
 *	<0 on error (if the stack if contains MAX_SYMLINK_DEPTH directories
 *		errno = ELOOP for detecting symbolic link loops
 */
static int push_path_on_stack(dir_stack *stack, const char *path)
{
    char *new_path;

    if (stack->count >= MAX_SYMLINK_DEPTH) {
        /* return potential symbolic link loop */
        errno = ELOOP;
        return -1;
    }

    new_path = strdup(path);
    if (new_path == NULL) {
        return -2;
    }

    stack->stack[stack->count].original_ptr = new_path;
    stack->stack[stack->count].cur_position = new_path;
    ++stack->count;

    return 0;
}

/*
 * get_next_component
 * 	Returns the next directory component that was pushed on the stack.
 * 	This value is always a local entry in the current directory (contains
 * 	no "/"), unless this is the first call to get_next_component after
 * 	an absolute path name was pushed on the stack, in which case the root
 * 	directory path ("/") is returned.
 *
 * 	The pointer to path is valid until the next call to get_next_component,
 * 	or destroy_dir_stack is called.  The dir_stack owns the memory pointed
 * 	by *path.
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get the next component
 *	path
 *		pointer to a pointer to store the next component
 * returns
 *	0  on sucess
 *	<0 on stack empty
 */
static int get_next_component(dir_stack *stack, const char **path)
{
    while (stack->count > 0) {
        if (!*stack->stack[stack->count - 1].cur_position) {
            /* current top is now empty, delete it, and try again */
            --stack->count;
            free(stack->stack[stack->count].original_ptr);
        } else {
            /* get beginning of the path */
            char *cur_path = stack->stack[stack->count - 1].cur_position;

            /* find the end */
            char *dir_sep_pos = strchr(cur_path, '/');

            *path = cur_path;
            if (dir_sep_pos) {
                if (dir_sep_pos ==
                    stack->stack[stack->count - 1].original_ptr) {
                    /* at the beginning of an absolute path, return root directory */
                    *path = "/";
                } else {
                    /* terminate the path returned just after the end of the component */
                    *dir_sep_pos = '\0';
                }
                /* set the pointer for the next call */
                stack->stack[stack->count - 1].cur_position =
                    dir_sep_pos + 1;
            } else {
                /* at the last component, set the pointer to the end of the string */
                stack->stack[stack->count - 1].cur_position +=
                    strlen(cur_path);
            }

            /* return success */
            return 0;
        }
    }

    /* return stack was empty */
    return -1;
}

/*
 * is_stack_empty
 * 	Returns true if the stack is empty, false otherwise.
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get the next component
 * returns
 *	0  if stack is not empty
 *	1  is stack is empty
 */
static int is_stack_empty(dir_stack *stack)
{
    /* since the empty items are not removed until the next call to
     * get_next_component(), we need to check all the items on the stack
     * and if any of them are not empty, return false, otherwise it truely
     * is empty.
     */
    int cur_head = stack->count - 1;
    while (cur_head >= 0) {
        if (*stack->stack[cur_head--].cur_position != '\0') {
            return 0;
        }
    }

    return 1;
}

/*
 * safe_is_path_trusted
 *
 * 	Returns the trustedness of the path.
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following symbolic links as discovered.  Non-directory entries in a
 * 	sticky bit directory are not trusted as untrusted users could have
 * 	hard linked an old file at that name.
 *
 * 	PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporaray file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	PATH_TRUSTED is returned only if the path given always referes to
 * 		the same object and the object referred can not be modified.
 * 	PATH_TRUSTED_CONFIDENTIAL is returned if the path is PATH_TRUSTED
 * 		nad the object referred to can not be read by untrusted
 * 		users.  This assumes the permissions on the object were
 * 		always strong enough to return this during the life of
 * 		the object.  This confidentiality is only based on the the
 * 		actual object, not the containing directories (for example a
 * 		file with weak permissions in a confidential directory is not
 * 		confidential).
 *
 * 	This function is not thread safe if other threads depend on the value
 * 	of the current working directory as it changes the current working
 * 	directory while checking the path and restores it on exit.
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
int
safe_is_path_trusted(const char *pathname, id_range_list *trusted_uids,
                     id_range_list *trusted_gids)
{
    int r;
    int status = -1;
    int saved_dir;
    dir_stack paths;
    const char *path;

    init_dir_stack(&paths);

    saved_dir = open(".", O_RDONLY);
    if (saved_dir == -1) {
        goto restore_dir_and_exit;
    }

    if (*pathname != '/') {
        /* relative path */
        status =
            is_current_working_directory_trusted(trusted_uids,
                                                 trusted_gids);
        if (status <= PATH_UNTRUSTED) {
            /* an error or untrusted current working directory */
            goto restore_dir_and_exit;
        }
    }

    /* start the stack with the pathname given */
    if (push_path_on_stack(&paths, pathname)) {
        status = -1;
        goto restore_dir_and_exit;
    }

    while (!get_next_component(&paths, &path)) {
        struct stat stat_buf;
        mode_t m;
        int prev_status;

        if (*path == '\0' || !strcmp(path, ".")) {
            /* current directory, already checked */
            continue;
        }
        if (!strcmp(path, "/")) {
            /* restarting at root, trust what is above root */
            status = PATH_TRUSTED;
        }

        prev_status = status;

        /*
         * At this point if the directory component is '..', then the status
         * should be set to be that of the grandparent directory, '../..',
         * for the code below to work, which would require either recomputing
         * the value, or keeping a cache of the value (which could then be used
         * to get the trust level of '..' directly).
         *
         * This is not necessary at this point in the processing we know that
         *   1) '..' is a directory
         *   2) '../..' trust was not PATH_UNTRUSTED
         *   3) the current trust level (status) is not PATH_UNTRUSTED
         *   4) the trust matrix rows are the same, when the parent is not
         *      PATH_UNTRUSTED
         * So not chnaging status will still result in the correct value
         *
         * WARNING: If any of these assumptions, change this will need to change.
         */

        /* check the next component in the path */
        r = lstat(path, &stat_buf);
        if (r == -1) {
            status = -1;
            goto restore_dir_and_exit;
        }

        /* compute the new trust, from the parent trust and the current directory */
        status =
            is_component_in_dir_trusted(status, &stat_buf, trusted_uids,
                                        trusted_gids);
        if (status <= PATH_UNTRUSTED) {
            goto restore_dir_and_exit;
        }

        m = stat_buf.st_mode;

        if (S_ISLNK(m)) {
            /* symbolic link found */
            int link_path_len = stat_buf.st_size;
            int readlink_len;
            char *link_path = malloc(link_path_len + 1);

            if (link_path == 0) {
                fatal_error_exit(1, "malloc failed in link_path");
            }

            /* get the link's referent */
            readlink_len = readlink(path, link_path, link_path_len + 1);
            if (readlink_len == -1) {
                free(link_path);
                status = -1;
                goto restore_dir_and_exit;
            }

            if (readlink_len >= link_path_len + 1) {
                free(link_path);
                status = -1;
                errno = ENAMETOOLONG;
                goto restore_dir_and_exit;
            }

            link_path[readlink_len] = '\0';

            /* add the path of the referent to the stack */
            if (push_path_on_stack(&paths, link_path)) {
		free(link_path);
                status = -1;
                goto restore_dir_and_exit;
            }

            /* restore value to that of containing directory */
            status = prev_status;

            free(link_path);

            continue;
        } else if (!is_stack_empty(&paths)) {
            /* more components remaining, change directory
             * it is not a sym link, so it must be a directory, or an error
             */
            r = chdir(path);
            if (r == -1) {
                status = -1;
                goto restore_dir_and_exit;
            }
        }
    }

  restore_dir_and_exit:

    /* restore original directory if needed and return value */

    destroy_dir_stack(&paths);

    if (saved_dir != -1) {
        r = fchdir(saved_dir);
        if (r == -1) {
            status = -1;
        }
        r = close(saved_dir);
        if (r == -1) {
            status = -1;
        }
    }

    return status;
}

/*
 * safe_is_path_trusted_fork
 *
 * 	Returns the trustedness of the path.
 *
 * 	This functino is thread/signal handler safe in that it does not change
 * 	the current working directory.  It does fork the process to return do
 * 	the check, which changes the new process's current working directory as
 * 	it does the checks by calling safe_is_path_trusted().
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following symbolic links as discovered.  Non-directory entries in a
 * 	sticky bit directory are not trusted as untrusted users could have
 * 	hard linked an old file at that name.
 *
 * 	PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporaray file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	PATH_TRUSTED is returned only if the path given always referes to
 * 		the same object and the object referred can not be modified.
 * 	PATH_TRUSTED_CONFIDENTIAL is returned if the path is PATH_TRUSTED
 * 		nad the object referred to can not be read by untrusted
 * 		users.  This assumes the permissions on the object were
 * 		always strong enough to return this during the life of
 * 		the object.  This confidentiality is only based on the the
 * 		actual object, not the containing directories (for example a
 * 		file with weak permissions in a confidential directory is not
 * 		confidential).
 *
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
int
safe_is_path_trusted_fork(const char *pathname,
                          id_range_list *trusted_uids,
                          id_range_list *trusted_gids)
{
    int r;
    int status = 0;
    pid_t pid;
    int fd[2];

    sigset_t no_sigchld_mask;
    sigset_t save_mask;
    sigset_t all_signals_mask;

    struct result_struct {
        int status;
        int err;
    };

    struct result_struct result;

    /* create a mask to block all signals */
    r = sigfillset(&all_signals_mask);
    if (r < 0) {
        return -1;
    }

    /* set no_sigchld_mask to current mask with SIGCHLD */
    r = sigprocmask(SIG_BLOCK, NULL, &no_sigchld_mask);
    if (r < 0) {
        return -1;
    }
    r = sigaddset(&no_sigchld_mask, SIGCHLD);
    if (r < 0) {
        return -1;
    }

    /* block all signals to prevent a signal handler from running in our
     * child */
    r = sigprocmask(SIG_SETMASK, &all_signals_mask, &save_mask);
    if (r < 0) {
        return -1;
    }

    /* create a pipe to communicate the results back */
    r = pipe(fd);
    if (r < 0) {
        goto restore_mask_and_exit;
    }

    pid = fork();
    if (pid < 0) {
        status = -1;
        goto restore_mask_and_exit;
    } else if (pid == 0) {
        /* in the child process */

        char *buf = (char *) &result;
        ssize_t bytes_to_send = sizeof result;

        /* close the read end of the pipe */
        r = close(fd[0]);

        result.status =
            safe_is_path_trusted(pathname, trusted_uids, trusted_gids);
        result.err = errno;

        /* send the result and errno back, handle EINTR and partial writes */
        while (bytes_to_send > 0) {
            ssize_t bytes_sent = write(fd[1], buf, bytes_to_send);
            if (bytes_sent != bytes_to_send && errno != EINTR) {
                status = -1;
                break;
            } else if (bytes_sent > 0) {
                buf += bytes_sent;
                bytes_to_send -= bytes_sent;
            }
        }

        r = close(fd[1]);
        if (r < 0) {
            status = -1;
        }

        /* do not do any cleanup (atexit, etc) leave it to the parent */
        _exit(status);
    } else {
        /* in the parent process */

        char *buf = (char *) &result;
        ssize_t bytes_to_read = sizeof result;
        int child_status;

        /* allow all signals except SIGCHLD from being sent,
         * so the application does not see our child die */
        r = sigprocmask(SIG_SETMASK, &no_sigchld_mask, NULL);
        if (r < 0) {
            status = -1;
        }

        /* close the write end of the pipe */
        r = close(fd[1]);
        if (r < 0) {
            status = -1;
        }

        result.err = 0;

        /* read the result and errno, handle EINTR and partial reads */
        while (status != -1 && bytes_to_read > 0) {
            ssize_t bytes_read = read(fd[0], buf, bytes_to_read);
            if (bytes_read != bytes_to_read && errno != EINTR) {
                status = -1;
            } else if (bytes_read > 0) {
                buf += bytes_read;
                bytes_to_read -= bytes_read;
            } else if (bytes_read == 0) {
                /* EOF - pipe was closed before all the data was written */
                status = -1;
            }
        }

        if (status == 0) {
            /* successfully got result and errno from child set them */
            status = result.status;
            errno = result.err;
        }

        r = close(fd[0]);
        if (r < 0) {
            status = -1;
        }

        while (waitpid(pid, &child_status, 0) < 0) {
            if (errno != EINTR) {
                status = -1;
                goto restore_mask_and_exit;
            }
        }

        if (!WIFEXITED(child_status) && WEXITSTATUS(child_status) != 0) {
            status = -1;
        }
    }

  restore_mask_and_exit:

    r = sigprocmask(SIG_SETMASK, &save_mask, NULL);
    if (r < 0) {
        status = r;
    }

    return status;
}

static int
append_dir_entry_to_path(char *path, char **path_end, char *buf_end,
                         const char *name)
{
    char *old_path_end = *path_end;

    if (*name == '\0' || !strcmp(name, ".")) {
        /* current working directory name, skip */
        return 0;
    }

    if (!strcmp(name, "/")) {
        /* reset the path, if name is the root directory */
        *path_end = path;
    }

    if (!strcmp(name, "..")) {
        /* if path is empty, skip and append ".." later */
        if (path != *path_end) {
            /* find the beginning of the last component */
            char *last_comp = *path_end;
            while (last_comp > path && last_comp[-1] != '/') {
                --last_comp;
            }

            if (strcmp(last_comp, "") && strcmp(last_comp, ".")
                && strcmp(last_comp, "..")) {
                /* if not current or parent directory, remove component */
                *path_end = last_comp;
                if (last_comp > path) {
                    --*path_end;
                }
                **path_end = '\0';
            }

            return 0;
        }
    }

    if (*path_end != path && (*path_end)[-1] != '/') {
        if (*path_end + 1 >= buf_end) {
            errno = ENAMETOOLONG;
            return -1;
        }

        *(*path_end)++ = '/';
        *(*path_end) = '\0';
    }

    /* copy component name to the end, except null */
    while (*path_end < buf_end && *name) {
        *(*path_end)++ = *name++;
    }

    if (*name) {
        /* not enough room for path, return error */
        errno = ENAMETOOLONG;
        *old_path_end = '\0';
        return -1;
    }

    /* null terminate the path */
    **path_end = '\0';

    return 0;
}

/*
 * safe_is_path_trusted_r
 *
 * 	Returns the trustedness of the path.
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following symbolic links as discovered.  Non-directory entries in a
 * 	sticky bit directory are not trusted as untrusted users could have
 * 	hard linked an old file at that name.
 *
 * 	PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporaray file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	PATH_TRUSTED is returned only if the path given always referes to
 * 		the same object and the object referred can not be modified.
 * 	PATH_TRUSTED_CONFIDENTIAL is returned if the path is PATH_TRUSTED
 * 		nad the object referred to can not be read by untrusted
 * 		users.  This assumes the permissions on the object were
 * 		always strong enough to return this during the life of
 * 		the object.  This confidentiality is only based on the the
 * 		actual object, not the containing directories (for example a
 * 		file with weak permissions in a confidential directory is not
 * 		confidential).
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *	<0  on error
 *	0   PATH_UNTRUSTED
 *	1   PATH_TRUSTED_STICKY_DIR
 *	2   PATH_TRUSTED
 *	3   PATH_TRUSTED_CONFIDENTIAL 
 */
int
safe_is_path_trusted_r(const char *pathname, id_range_list *trusted_uids,
                       id_range_list *trusted_gids)
{
    int r;
    int status = -1;
    dir_stack paths;
    const char *comp_name;
    char path[PATH_MAX];
    char *path_end = path;
    char *prev_path_end;

    init_dir_stack(&paths);

    if (*pathname != '/') {
        /* relative path */
        status =
            is_current_working_directory_trusted_r(trusted_uids,
                                                   trusted_gids);
        if (status <= PATH_UNTRUSTED) {
            /* an error or untrusted current working directory */
            goto cleanup_and_exit;
        }
    }

    /* start the stack with the pathname given */
    if (push_path_on_stack(&paths, pathname)) {
        status = -1;
        goto cleanup_and_exit;
    }

    while (!get_next_component(&paths, &comp_name)) {
        struct stat stat_buf;
        mode_t m;
        int prev_status;

        if (*comp_name == '\0' || !strcmp(comp_name, ".")) {
            /* current directory, already checked */
            continue;
        }
        if (!strcmp(comp_name, "/")) {
            /* restarting at root, trust what is above root */
            status = PATH_TRUSTED;
        }

        prev_path_end = path_end;
        prev_status = status;

        r = append_dir_entry_to_path(path, &path_end, path + sizeof(path),
                                     comp_name);
        if (r == -1) {
            status = -1;
            goto cleanup_and_exit;
        }

        /*
         * At this point if the directory component is '..', then the status
         * should be set to be that of the grandparent directory, '../..',
         * for the code below to work, which would require either recomputing
         * the value, or keeping a cache of the value (which could then be used
         * to get the trust level of '..' directly).
         *
         * This is not necessary at this point in the processing we know that
         *   1) '..' is a directory
         *   2) '../..' trust was not PATH_UNTRUSTED
         *   3) the current trust level (status) is not PATH_UNTRUSTED
         *   4) the trust matrix rows are the same, when the parent is not
         *      PATH_UNTRUSTED
         * So not chnaging status will still result in the correct value
         *
         * WARNING: If any of these assumptions, change this will need to change.
         */

        /* check the next component in the path */
        r = lstat(path, &stat_buf);
        if (r == -1) {
            status = -1;
            goto cleanup_and_exit;
        }

        /* compute the new trust, from the parent trust and the current directory */
        status =
            is_component_in_dir_trusted(status, &stat_buf, trusted_uids,
                                        trusted_gids);
        if (status <= PATH_UNTRUSTED) {
            goto cleanup_and_exit;
        }

        m = stat_buf.st_mode;

        if (S_ISLNK(m)) {
            /* symbolic link found */
            int link_path_len = stat_buf.st_size;
            int readlink_len;
            char *link_path = malloc(link_path_len + 1);

            if (link_path == 0) {
                fatal_error_exit(1, "malloc failed in link_path");
            }

            /* get the link's referent */
            readlink_len = readlink(path, link_path, link_path_len + 1);
            if (readlink_len == -1) {
                free(link_path);
                status = -1;
                goto cleanup_and_exit;
            }

            if (readlink_len >= link_path_len + 1) {
                free(link_path);
                status = -1;
                errno = ENAMETOOLONG;
                goto cleanup_and_exit;
            }

            link_path[readlink_len] = '\0';

            /* add path to the stack */
            if (push_path_on_stack(&paths, link_path)) {
		free(link_path);
                status = -1;
                goto cleanup_and_exit;
            }

            free(link_path);

            /* restore values to the containing directory */
            status = prev_status;
            path_end = prev_path_end;
            *path_end = '\0';
        } else {
            if (!is_stack_empty(&paths) && !S_ISDIR(stat_buf.st_mode)) {
                status = -1;
                errno = ENOTDIR;
                goto cleanup_and_exit;
            }
        }
    }

  cleanup_and_exit:

    /* restore original directory if needed and return value */
    destroy_dir_stack(&paths);

    /* if this algorithm failed because the pathname was too long,
     * try the fork version on the same pathname as it can handle all valid paths
     */
    if (status == -1 && errno == ENAMETOOLONG) {
        status =
            safe_is_path_trusted_fork(pathname, trusted_uids,
                                      trusted_gids);
    }

    return status;
}

/***********************************************************************
 *
 * Functions to walk a directory tree
 *
 ***********************************************************************/

#if !HAVE_DIRFD
/*
 * opendir_with_fd
 * parameters
 *  path
 *      the path to the dir to open
 *  dir_ptr
 *      receives the DIR* from the opendir
 *  fd_ptr
 *      receives an FD for this same directory
 *  stat_buf
 *      receives stat buffer for this directory
 *  returns
 *    0 on success
 *   -1 on error
 */
static int
opendir_with_fd(const char *path,
                DIR ** dir_ptr, int *fd_ptr, struct stat *stat_buf)
{
    int fd = -1;
    struct stat tmp_stat_buf;
    DIR *dir = NULL;
    dev_t dev;
    ino_t ino;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        goto OPENDIR_WITH_FD_FAILURE;
    }
    if (fstat(fd, &tmp_stat_buf) == -1) {
        goto OPENDIR_WITH_FD_FAILURE;
    }
    dir = opendir(path);
    if (dir == NULL) {
        goto OPENDIR_WITH_FD_FAILURE;
    }
    dev = tmp_stat_buf.st_dev;
    ino = tmp_stat_buf.st_ino;
    if (lstat(".", &tmp_stat_buf) == -1) {
        goto OPENDIR_WITH_FD_FAILURE;
    }
    if ((tmp_stat_buf.st_dev != dev) || (tmp_stat_buf.st_ino != ino)) {
        goto OPENDIR_WITH_FD_FAILURE;
    }

    *dir_ptr = dir;
    *fd_ptr = fd;
    if (stat_buf != NULL) {
        memcpy(stat_buf, &tmp_stat_buf, sizeof(struct stat));
    }
    return 0;

  OPENDIR_WITH_FD_FAILURE:
    if (fd != -1) {
        close(fd);
    }
    if (dir != NULL) {
        closedir(dir);
    }
    return -1;
}
#endif

/*
 * safe_open_no_follow
 *      opens the given path and optionally returns stat information. this
 *      function ensures that the _last_ component in this path is not a
 *      symlink, but makes no such promise for other path components.
 *      this function can only be used to obtain an O_RDONLY FD
 *
 * parameters
 *      path
 *              the path to open
 *      fd_ptr
 *              on success, this location is set to the open FD if
 *              the path was opened, or -1 if a symlink was detected.
 *              the location may be altered even on failure
 *      stat_buf
 *              optionally, a pointer to a stat buffer to receive info on
 *              the opened path. the buffer may be altered even if noting
 *              is opened
 * returns
 *      0 on success (which means either the path was opened or a symlink
 *        was detected)
 *     -1 on error
 */
int
safe_open_no_follow(const char* path, int* fd_ptr, struct stat* st)
{
    dev_t dev;
    ino_t ino;
    struct stat buf;

    if (st == NULL) {
        st = &buf;
    }

    *fd_ptr = open(path, O_RDONLY | O_NONBLOCK);
    if (*fd_ptr == -1) {
        return -1;
    }

    if (fstat(*fd_ptr, st) == -1) {
        close(*fd_ptr);
        return -1;
    }

    dev = st->st_dev;
    ino = st->st_ino;

    if (lstat(path, st) == -1) {
        close(*fd_ptr);
        return -1;
    }

    if ((st->st_dev != dev) || (st->st_ino != ino)) {
         close(*fd_ptr);
         *fd_ptr = -1;
    }

    return 0;
}

/*
 * chdir_no_follow
 *
 * parameters
 *      dir_name
 *              a directory name, relative to the current dir, to change to
 * returns
 *       0 on success
 *      -1 on error
 */
static int
chdir_no_follow(const char* dir_name)
{
    int fd;
    struct stat stat_buf;

    if (safe_open_no_follow(dir_name, &fd, &stat_buf)) {
        return -1;
    }
    if (fd == -1) {
        /* symlink; bail */
        return -1;
    }

    if (fchdir(fd) == -1) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/*
 * do_dir_contents_one_fd
 *
 * parameters
 * 	func
 * 		this is the call back function to call for each entry
 * 	data
 * 		this is a block of data that is passed to func
 * returns
 *	 0  on success
 *	-1  on error
 */
static int do_dir_contents_one_fd(safe_dir_walk_func func, void *data)
{
    DIR *dir;
    int dir_fd;
    int r;
    int status = 0;
    struct dirent *de;
    string_list dirs;
    struct stat stat_buf;
    struct stat *statp;
    dev_t cur_dir_dev;
    ino_t cur_dir_ino;

    init_string_list(&dirs);

#if HAVE_DIRFD
    dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }
    dir_fd = dirfd(dir);
    r = fstat(dir_fd, &stat_buf);
    if (r == -1) {
        status = -1;
    }
#else
    if (opendir_with_fd(".", &dir, &dir_fd, &stat_buf) == -1) {
        return -1;
    }
    close(dir_fd);
#endif

    cur_dir_dev = stat_buf.st_dev;
    cur_dir_ino = stat_buf.st_ino;

    if (status == 0) {
        while ((de = readdir(dir)) != 0) {
            statp = &stat_buf;

            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                continue;
            }

            r = lstat(de->d_name, statp);
            if (r == -1) {
                statp = 0;
            }

            if (statp && S_ISDIR(statp->st_mode)) {
                char *dir_name = strdup(de->d_name);
                if (dir_name == NULL) {
                    status = -1;
                    break;
                }
                add_string_to_list(&dirs, dir_name);
            } else {
                status = func(de->d_name, statp, data);
                if (status != 0) {
                    break;
                }
            }
        }
    }

    r = closedir(dir);
    if (status == 0 && r == -1) {
        status = -1;
    }

    if (status == 0) {
        char **dirptr = null_terminated_list_from_string_list(&dirs);

        while (*dirptr) {

            status = chdir_no_follow(*dirptr);
            if (status == -1) {
                break;
            }

            status = do_dir_contents_one_fd(func, data);

            r = chdir("..");
            if (status != 0 || r == -1) {
                if (status == 0) {
                    status = -1;
                }
                break;
            }

            r = lstat(".", &stat_buf);
            if (r == -1) {
                status = -1;
                break;
            }

            /* check to make sure we are in the right directory */
            if (cur_dir_dev != stat_buf.st_dev
                || cur_dir_ino != stat_buf.st_ino) {
                status = -1;
                break;
            }

            statp = &stat_buf;
            r = lstat(*dirptr, statp);
            if (r == -1) {
                statp = 0;
            }

            status = func(*dirptr, statp, data);
            if (status != 0) {
                break;
            }

            ++dirptr;
        }
    }

    destroy_string_list(&dirs);

    if (status == 0) {
        status = r;
    }

    return status;
}

/*
 * do_dir_contents
 *
 * parameters
 * 	func
 * 		this is the call back function to call for each entry
 * 	data
 * 		this is a block of data that is passed to func
 * 	num_fds
 * 		number of file descriptors to use, calls slower
 * 		do_dir_contents_one_fd() if it is going to exceed this
 * returns
 *	 0  on success
 *	-1  on error
 */
static int
do_dir_contents(safe_dir_walk_func func, void *data, int num_fds)
{
    DIR *dir;
    int dir_fd;
    int r;
    int status = 0;
    struct dirent *de;

    if (num_fds <= 1) {
        return do_dir_contents_one_fd(func, data);
    }
#if HAVE_DIRFD
    dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }
    dir_fd = dirfd(dir);
#else
    if (opendir_with_fd(".", &dir, &dir_fd, NULL) == -1) {
        return -1;
    }
#endif

    while ((de = readdir(dir)) != 0) {
        struct stat stat_buf;
        struct stat *statp = &stat_buf;

        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
            continue;
        }

        r = lstat(de->d_name, statp);
        if (r == -1) {
            statp = 0;
        }

        if (statp && S_ISDIR(statp->st_mode)) {
            status = chdir_no_follow(de->d_name);
            if (status == -1) {
                break;
            }

            status = do_dir_contents(func, data, num_fds - 1);

            r = fchdir(dir_fd);
            if (status != 0 || r != 0) {
                if (status == 0) {
                    status = r;
                }
                break;
            }
        }

        status = func(de->d_name, statp, data);
        if (status != 0) {
            break;
        }
    }

    r = closedir(dir);
#if !HAVE_DIRFD
    close(dir_fd);
#endif

    if (status == 0) {
        status = r;
    }

    return status;
}

/*
 * safe_dir_walk
 *
 * parameters
 * 	func
 * 		this is the call back function to call for each entry
 * 	data
 * 		this is a block of data that is passed to func
 * 	num_fds
 * 		number of file descriptors to use, calls slower
 * 		do_dir_contents_one_fd() if it is going to exceed this
 * returns
 *	 0  on success
 *	-1  on error
 */
int
safe_dir_walk(const char *path, safe_dir_walk_func func, void *data,
              int num_fds)
{
    int r;
    int saved_dir;
    int status = -1;

    /* save the current directory */
    saved_dir = open(".", O_RDONLY);
    if (saved_dir == -1) {
        goto cleanup_and_exit;
    }

    r = chdir_no_follow(path);
    if (r == -1) {
        goto cleanup_and_exit;
    }

    status = do_dir_contents(func, data, num_fds - 1);

  cleanup_and_exit:

    /* restore the original current working directory */
    if (saved_dir != -1) {
        r = fchdir(saved_dir);
        if (!status && r == -1) {
            status = -1;
        }

        r = close(saved_dir);
        if (!status && r == -1) {
            status = -1;
        }
    }

    return status;
}
