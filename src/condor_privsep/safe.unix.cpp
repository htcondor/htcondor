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

#include "safe.unix.h"

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
 * nonfatal_write
 *	This function tries to print a some information to the stream setup by
 *	setup_err_stream or stderr if there is none.
 * parameters
 *	fmt, ...
 *		format and parameters as expected by printf
 * returns
 *	nothing
 */
void nonfatal_write(const char *fmt, ...)
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
}

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
    if (r == -1) {
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

    tmp = (char *) malloc(strlen(var) + strlen(val) + 2);
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
        free(tz);
        return -1;
    }

    /* set PATH and TZ */
    r = setenv(path_name, path_value, 1);
    if (r == -1) {
        free(tz);
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
int safe_close_fds_except(safe_id_range_list *ids)
{
    int fd;
    int open_max = get_open_max();
    if (open_max == -1) {
        /* could not find the max, panic! */
        return -1;
    }

    for (fd = 0; fd < open_max; ++fd) {
        if (!safe_is_id_in_list(ids, fd)) {
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
 * 	-13  the tracking group was not safe
 */
static int
safe_checks_and_set_gids(uid_t uid,
                         gid_t tracking_gid,
                         safe_id_range_list *safe_uids,
                         safe_id_range_list *safe_gids,
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
    if (!safe_is_id_in_list(safe_uids, uid)) {
        return -1;
    }

    /* check tracking GID, if given, is safe */
    if ((tracking_gid != 0) && !safe_is_id_in_list(safe_gids, tracking_gid)) {
		return -13;
	}

    /* check uid is in the password file */
    pw = getpwuid(uid);
    if (pw == NULL) {
        return -2;
    }

    /* get primary group */
    gid = pw->pw_gid;

    /* check primary group is in safe_gids */
    if (!safe_is_id_in_list(safe_gids, gid)) {
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
        if (!safe_is_id_in_list(safe_gids, gids[i])) {
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

char const *
safe_switch_to_uid_strerror(int r)
{
	switch(r) {
	case 0: return "success";
	case -1: return "uid not safe";
	case -2: return "uid not in password file";
	case -3: return "primary group not safe";
	case -4: return "failed to set supplementary groups";
	case -5: return "failed to get number of supplementary groups";
	case -6: return "malloc failed";
	case -7: return "failed to get supplementary groups";
	case -8: return "a supplementary group was not safe";
	case -9: return "failed to set real gid";
	case -10: return "failed to set real uid";
	case -11: return "id did not change properly";
	case -12: return "failed to insert tracking GID";
	case -13: return "the tracking group was not safe";
	}
	return "";
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
                   safe_id_range_list *safe_uids,
                   safe_id_range_list *safe_gids)
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
safe_switch_effective_to_uid(uid_t uid, safe_id_range_list *safe_uids,
                             safe_id_range_list *safe_gids)
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
                  safe_id_range_list *safe_uids,
                  safe_id_range_list *safe_gids,
                  const char *exec_name,
                  char **args,
                  char **env,
                  safe_id_range_list *keep_open_fds,
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
        char const *reason = safe_switch_to_uid_strerror(r);
        if( !reason ) {
            reason = "";
        }
        fatal_error_exit(1, "error switching to uid %lu, tracking gid %lu: code %d %s",
                         (unsigned long) uid,tracking_gid,r,reason);
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
    if (!safe_is_id_in_list(keep_open_fds, 0)) {
        r = safe_open_std_file(0, stdin_filename);
        if (r == -1) {
            fatal_error_exit(1, "error opening stdin");
        }
    }

    if (!safe_is_id_in_list(keep_open_fds, 1)) {
        r = safe_open_std_file(1, stdout_filename);
        if (r == -1) {
            fatal_error_exit(1, "error opening stdout");
        }
    }

    if (!safe_is_id_in_list(keep_open_fds, 2)) {
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
#else
    if (is_std_univ) {}
#endif

    /* finally do the exec */
    r = execve(exec_name, (char **) args, (char **) env);
    if (r == -1) {
        fatal_error_exit(1, "error exec'ing: %s", strerror(errno));
    }

    return 0;
}

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
	if (errno == ENOENT || errno == EACCES) {
	    /* path could have been a dangling sym link
	    * check for symlink and return 0 with fd = -1
	    */
	    int open_errno = errno;
	    if (lstat(path, st) != -1 && S_ISLNK(st->st_mode)) {
		return 0;
	    }
	    errno = open_errno;
	}

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
