/*
 * safefile package    http://www.cs.wisc.edu/~kupsch/safefile
 *
 * Copyright 2007-2008, 2010-2011 James A. Kupsch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
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

#include "safe_id_range_list.h"



/***********************************************************************
 *
 * Initialize global variables
 *
 ***********************************************************************/

const id_t safe_err_id	= (id_t)-1;


/***********************************************************************
 *
 * Initialize global variables
 *
 ***********************************************************************/

typedef struct safe_id_range_list_elem
{
    id_t min_value;
    id_t max_value;
}  safe_id_range_list_elem;




/***********************************************************************
 *
 * Functions for manipulating id range lists
 *
 ***********************************************************************/


/*
 * safe_init_id_range_list
 * 	Initialize an id_range_list structure.
 * parameters
 * 	list
 * 		pointer to an id range list
 * returns
 * 	0   for success
 * 	-1  on failure (errno == ENOMEM or EINVAL)
 */
int safe_init_id_range_list(safe_id_range_list *list)
{
    if (list == NULL)  {
	errno = EINVAL;
	return -1;
    }

    list->count = 0;
    list->capacity = 10;
    list->list = (safe_id_range_list_elem*)malloc(list->capacity * sizeof(list->list[0]));
    if (list->list == 0)  {
	errno = ENOMEM;
	return -1;
    }

    return 0;
}


/*
 * safe_add_id_range_to_list
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
 * 	0   for success
 * 	-1  on failure (errno == ENOMEM or EINVAL)
 */
int safe_add_id_range_to_list(safe_id_range_list *list, id_t min_id, id_t max_id)
{
    if (list == NULL || min_id > max_id)  {
	errno = EINVAL;
	return -1;
    }

    if (list->count == list->capacity)  {
	size_t new_capacity = 10 + 11 * list->capacity / 10;
	safe_id_range_list_elem *new_list = (safe_id_range_list_elem*)malloc(new_capacity * sizeof(new_list[0]));
	if (new_list == 0)  {
	    errno = ENOMEM;
	    return -1;
	}
	memcpy(new_list, list->list, list->count * sizeof(new_list[0]));
	free(list->list);
	list->list = new_list;
	list->capacity = new_capacity;
    }

    list->list[list->count].min_value = min_id;
    list->list[list->count++].max_value = max_id;

    return 0;
}


/*
 * safe_add_id_to_list
 * 	Add the single id to the list.  This is the same as calling
 * 	safe_add_id_range_to_list with min_id and max_id set to id.
 * parameters
 * 	list
 * 		pointer to an id range list
 * 	id
 * 		the id to add
 * returns
 * 	0   for success
 * 	-1  on failure (errno == ENOMEM)
 */
int safe_add_id_to_list(safe_id_range_list *list, id_t id)
{
    return safe_add_id_range_to_list(list, id, id);
}


/*
 * safe_destroy_id_range_list
 * 	Destroy a id_range_list, including any memory have acquired.
 * parameters
 * 	list
 * 		pointer to id range list structure to destroy
 * returns
 * 	nothing
 */
void safe_destroy_id_range_list(safe_id_range_list *list)
{
    if (list == NULL)  {
	errno = EINVAL;
	return;
    }

    list->capacity = 0;
    list->count = 0;
    free(list->list);
    list->list = 0;
}


/*
 * safe_is_id_in_list
 * 	Check if the id is in one of the id ranges in the id range list.
 * parameters
 * 	list
 * 		pointer to an id range list
 * 	id
 * 		the id to check
 * returns
 * 	1	id is in the list
 * 	0	id is not in the list
 * 	-1	the list is NULL
 */
int safe_is_id_in_list(safe_id_range_list *list, id_t id)
{
    size_t i;

    if (list == NULL)  {
	errno = EINVAL;
	return -1;
    }

    for (i = 0; i < list->count; ++i)  {
	if (list->list[i].min_value <= id && id <= list->list[i].max_value)  {
	    return 1;
	}
    }

    return 0;
}


/*
 * safe_is_id_list_empty
 * 	Returns true if the id_range_list contains 0 ranges.
 * parameters
 * 	list
 * 		pointer to an id range list
 * returns
 * 	1	id is in the list
 * 	0	id is not in the list
 * 	-1	the list is NULL
 */
int safe_is_id_list_empty(safe_id_range_list *list)  {
    if (list == NULL)  {
	errno = EINVAL;
	return -1;
    }

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
static const char *skip_whitespace_const(const char *s)
{
    while (*s && isspace((unsigned char)*s))  {
	++s;
    }

    return s;
}


/*
 * name_to_error
 * 	Always return the err id (-1) and set errno to EINVAL.
 * parameters
 * 	name
 * 		unused
 * returns
 * 	safe_err_id and errno = EINVAL
 */
static id_t name_to_error(const char *name)
{
    (void)name;
    errno = EINVAL;
    return safe_err_id;
}


/*
 * name_to_uid
 * 	Return the uid matching the name if it exists.  If the name does
 * 	not exist or there was an error in getpwnam, safe_err_id is
 * 	returned and errno is set to a non-0 value.  If getpwnam fails
 * 	with errno unchanged, errno is set to EINVAL.
 * parameters
 * 	name
 * 		user name to lookup
 * returns
 * 	The uid corresponding to name if it exists.
 * 	safe_err_id if the name does not exist or an error occurs (errno is
 * 	set to the value from getpwnam or EINVAL if getpwnam does not set it).
 */
static id_t name_to_uid(const char *name)
{
    struct passwd	*pw	= getpwnam(name);

    errno = 0;

    if (!pw)  {
	if (errno == 0)  {
	    errno = EINVAL;
	}
	return safe_err_id;
    }

    return pw->pw_uid;
}


/*
 * name_to_gid
 * 	Return the uid matching the name if it exists.  If the name does
 * 	not exist or there was an error in getgrnam, safe_err_id is
 * 	returned and errno is set to a non-0 value.  If getgrnam fails
 * 	with errno unchanged, errno is set to EINVAL.
 * parameters
 * 	name
 * 		group name to lookup
 * returns
 * 	The uid corresponding to name if it exists.
 * 	safe_err_id if the name does not exist or an error occurs (errno is
 * 	set to the value from getgrnam or EINVAL if getgrnam does not set it).
 */
static id_t name_to_gid(const char *name)
{
    struct group	*gr	= getgrnam(name);

    errno = 0;

    if (!gr)  {
	if (errno == 0)  {
	    errno = EINVAL;
	}
	return safe_err_id;
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
typedef id_t (*lookup_func)(const char*);

static int strto_id(id_t *id, const char *value, const char **endptr, lookup_func lookup)
{
    const char	*endp;
    const char	*id_begin;

    if (id == NULL || value == NULL || lookup == NULL)  {
	errno = EINVAL;
	if (id)  {
	    *id = safe_err_id;
	}
	return -1;
    }

    endp = value;

    id_begin = skip_whitespace_const(value);

    errno = 0;

    if (isdigit((unsigned char)*id_begin))  {
	/* is numeric form, parse as a number */
	char *e;
	*id = (id_t)strtoul(id_begin, &e, 10);
	endp = e;
    }  else if (*id_begin)  {
	/* is not numeric, parse as a name using lookup function */
	char *id_name;
	size_t id_len;
	char small_buf[16];	/* should be big enough to hold most names */

	/* find end - name can contain anything except whitespace and colons */
	endp = id_begin;
	while (*endp && !isspace((unsigned char)*endp) && *endp != ':')  {
	    ++endp;
	}

	id_len = (size_t)(endp - id_begin);

	if (id_len == 0)  {
	    errno = EINVAL;
	    *id = safe_err_id;
	    if (endptr)  {
		*endptr = endp;
	    }
	    return -1;
	}  else if (id_len < sizeof(small_buf))  {
	    /* use small_buf as the id fits */
	    id_name = small_buf;
	}  else  {
	    /* malloc a buffer as id is too large for small_buf */
	    id_name = (char*)malloc(id_len + 1);
	    if (id_name == NULL)  {
		errno = ENOMEM;
		*id = safe_err_id;
		if (endptr)  {
		    *endptr = endp;
		}
		return -1;
	    }
	}

	/* copy the id to the buffer */
	memcpy(id_name, id_begin, id_len);
	id_name[id_len] = '\0';

	*id = lookup(id_name);

	/* free buffer if malloc'd */
	if (id_name != small_buf)  {
	    free(id_name);
	}
    }  else  {
	/* value contains nothing parsable */
	*id = safe_err_id;
	errno = EINVAL;
    }

    if (endptr)  {
	*endptr = endp;
    }

    return 0;
}


/*
 * safe_strto_uid
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
uid_t safe_strto_uid(const char *value, const char **endptr)
{
    id_t	id;

    strto_id(&id, value, endptr, name_to_uid);

    return (uid_t)id;
}


/*
 * safe_strto_gid
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
gid_t safe_strto_gid(const char *value, const char **endptr)
{
    id_t	id;

    strto_id(&id, value, endptr, name_to_gid);

    return (gid_t)id;
}


/*
 * safe_strto_id
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
id_t safe_strto_id(const char *value, const char **endptr)
{
    id_t	id;

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
 * 	'-', a space must precede the '-' if the first <id> is in a non-numeric
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
static void strto_id_range(id_t *min_id, id_t *max_id, const char *value, const char **endptr, lookup_func lookup)
{
    const char *endp;
    strto_id(min_id, value, &endp, lookup);
    if (errno == 0 && value != endp)  {
	/* parsed min correctly, check for a '-' and max value */
	value = skip_whitespace_const(endp);
	if (*value == '-')  {
	    ++value;
	    value = skip_whitespace_const(value);
	    if (*value == '*')  {
		*max_id = UINT_MAX;
		endp = value + 1;
	    }  else  {
		strto_id(max_id, value, &endp, lookup);
	    }
	}  else  {
	    *max_id = *min_id;
	}
    }  else  {
	*max_id = *min_id;
    }

    if (endptr)  {
	*endptr = endp;
    }

    if (*min_id > *max_id)  {
	errno = EINVAL;
    }
}


/*
 * strto_id_list
 * 	Adds the ranges in the value to the list.  Ranges are as specified in
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
static void strto_id_list(safe_id_range_list *list, const char *value, const char **endptr, lookup_func lookup)
{
    const char * endp = value;

    if (list == NULL || value == NULL)  {
	errno = EINVAL;
	if (endptr)  {
	    *endptr = value;
	}
	return;
    }

    while (1)  {
	id_t	min_id;
	id_t	max_id;

	strto_id_range(&min_id, &max_id, value, &endp, lookup);
	if (errno != 0 || value == endp)  {
	    break;
	}

	safe_add_id_range_to_list(list, min_id, max_id);

	value = skip_whitespace_const(endp);
	if (*value == ':')  {
	    ++value;
	}  else  {
	    break;
	}
    }

    if (endptr)  {
	*endptr = endp;
    }
}


/*
 * safe_strto_id_list
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
void safe_strto_id_list(safe_id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_error);
}


/*
 * safe_parse_uid_list
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
void safe_strto_uid_list(safe_id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_uid);
}


/*
 * safe_parse_gid_list
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
void safe_strto_gid_list(safe_id_range_list *list, const char *value, const char **endptr)
{
    strto_id_list(list, value, endptr, name_to_gid);
}


/*
 * parse_id_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are converted to ids by looking the
 * 	name as a group name and returning its gid.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	It is an error if there is non-whitespace after the parsed value.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * returns
 * 	0       on success
 * 	-1	on error
 */
static int parse_id_list(safe_id_range_list *list, const char *value, lookup_func lookup)
{
    const char *endp;

    strto_id_list(list, value, &endp, lookup);

    if (errno != 0)  {
	return -1;
    }

    /* check if there is non-whitespace after the parse portion of value */
    endp = skip_whitespace_const(endp);
    if (*endp != '\0')  {
	return -1;
    }

    return 0;
}


/*
 * safe_parse_id_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	It is an error if there is non-whitespace after the parsed value.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * returns
 * 	0       on success
 * 	-1	on error
 */
int safe_parse_id_list(safe_id_range_list *list, const char *value)
{
    return parse_id_list(list, value, name_to_error);
}


/*
 * safe_parse_uid_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are converted to ids by looking the
 * 	name as a user name and returning its uid.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	It is an error if there is non-whitespace after the parsed value.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * returns
 * 	0       on success
 * 	-1	on error
 */
int safe_parse_uid_list(safe_id_range_list *list, const char *value)
{
    return parse_id_list(list, value, name_to_uid);
}


/*
 * safe_parse_gid_list
 * 	Parse the value and store the ranges in the range list in the list
 * 	structure.  Non-numeric ids are converted to ids by looking the
 * 	name as a group name and returning its gid.
 *
 * 	The value is parsed as described in strto_id_list.
 *
 * 	It is an error if any of the ranges contain an error.
 *
 * 	It is an error if there is non-whitespace after the parsed value.
 *
 * 	On error errno is set to a non-zero value including EINVAL and ERANGE.
 * 	On success errno is set to 0.
 * parameters
 * 	list
 * 		a pointer to a range list to have the ranges parsed added
 * 	value
 * 		pointer to the beginning of the string to find the name or
 * 		number
 * returns
 * 	0       on success
 * 	-1	on error
 */
int safe_parse_gid_list(safe_id_range_list *list, const char *value)
{
    return parse_id_list(list, value, name_to_gid);
}
