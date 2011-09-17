/*
 * safefile package    http://www.cs.wisc.edu/~kupsch/safefile
 *
 * Copyright 2007-2008 James A. Kupsch
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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef WIN32
#include <string.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <stdio.h>
#include "safe_fopen.h"
#include "safe_open.h"




/*
 * Internal helper function declarations
 */
static int stdio_mode_to_open_flag(const char* flags, int* mode, int create_file);
static FILE* safe_fdopen(int fd, const char* flags);


/*
 * Replacement functions for fopen.  These functions differ in the following
 * ways:
 *
 * 1) file creation is always done safely and the semantics are determined by
 *    which of the 4 functions is used.
 * 2) the default file permission is 0644 instead of 0666.  umask still applies.
 * 3) a file permissions argument exists for file creation (optional in C++)
 * 4) in the case of "a", the file is opened with O_APPEND
 * 5) EINVAL is returned if the filename or flags are a NULL pointer, or the
 *    first character of the flag is not a valid standard fopen flag value:  rwa
 * 6) EEXISTS is returned if the final component is a symbolic link in both
 *    the case of creating and of opening an existing file
 */


/* open existing file */
FILE* safe_fopen_no_create(const char* fn, const char* flags)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 0);
    int f;

    if (r != 0)  {
	return NULL;
    }

    /* remove O_CREAT, allow "w" and "a" w/o create */
    open_flags &= ~O_CREAT;

    f = safe_open_no_create(fn, open_flags);

    return safe_fdopen(f, flags);
}


/* create file, fail if it exists, don't follow symlink */
FILE* safe_fcreate_fail_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);
    int f;

    if (r != 0)  {
	return NULL;
    }

    f = safe_create_fail_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


/* create file if it doesn't exist, keep inode if it does */
FILE* safe_fcreate_keep_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);
    int f;

    if (r != 0)  {
	return NULL;
    }

    f = safe_create_keep_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


/* create file, replace file if exists */
FILE* safe_fcreate_replace_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);
    int f;

    if (r != 0)  {
	return NULL;
    }

    f = safe_create_replace_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


/* open existing file */
FILE* safe_fopen_no_create_follow(const char* fn, const char* flags)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 0);
    int f;

    if (r != 0)  {
	return NULL;
    }

    /* remove O_CREAT, allow "w" and "a" w/o create */
    open_flags &= ~O_CREAT;

    f = safe_open_no_create_follow(fn, open_flags);

    return safe_fdopen(f, flags);
}


/* create file if it doesn't exist, keep inode if it does */
FILE* safe_fcreate_keep_if_exists_follow(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);
    int f;

    if (r != 0)  {
	return NULL;
    }

    f = safe_create_keep_if_exists_follow(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


/*
 * Helper functions for us in safe_fcreate* and safe_fopen functions
 */

/*
 * create the flags needed by open from mode string passed to fopen
 * fail if the create_file is true and mode is "r" or "rb", or if the
 * mode does not start with one of the 15 valid modes:
 *   r rb r+ r+b rb+  w wb w+ w+b wb+  a ab a+ a+b ab+
 * also fail if the flags or mode pointers are NULL.
 */
int stdio_mode_to_open_flag(const char* flags, int* mode, int create_file)
{
    int		plus_flag = 0;
    char	mode_char;

    if (!flags || !mode)  {
	errno = EINVAL;
	return -1;
    }

    *mode = 0;
    mode_char = flags[0];

    if (mode_char == 'r' || mode_char == 'w' || mode_char == 'a')  {
	if (flags[1] == 'b')  {
	    plus_flag = flags[2] == '+';
	}  else  {
	    plus_flag = flags[1] == '+';
	}
    }  else  {
	/* invalid flags */
	errno = EINVAL;
	return -1;
    }

    /* it is an error if "r" is passed to one of the creation routines */
    if (create_file && mode_char == 'r')  {
	errno = EINVAL;
	return -1;
    }

    /* 'w' and 'a' imply O_CREAT */
    if (mode_char != 'r')  {
	*mode |= O_CREAT;
    }

    if (plus_flag)  {
	*mode |= O_RDWR;
    }  else if (mode_char == 'r')  {
	*mode |= O_RDONLY;
    }  else   {		/* mode_char == 'w' or 'a' */
	*mode |= O_WRONLY;
    }

    if (mode_char == 'a')  {
	*mode |= O_APPEND;
    }  else if (mode_char == 'w')  {
	*mode |= O_TRUNC;
    }

    /* deal with windows extra flags: b t S R T D */

#ifdef WIN32
#ifdef _O_BINARY
    if (strchr(flags, 'b'))  {
	*mode |= _O_BINARY;
    }
#endif

#ifdef _O_TEXT
    if (strchr(flags, 't'))  {
	*mode |= _O_TEXT;
    }
#endif

#ifdef _O_SEQUENTIAL
    if (strchr(flags, 'S'))  {
	*mode |= _O_SEQUENTIAL;
    }
#endif

#ifdef _O_RANDOM
    if (strchr(flags, 'R'))  {
	*mode |= _O_RANDOM;
    }
#endif

#ifdef _O_SHORT_LIVED
    if (strchr(flags, 'T'))  {
	*mode |= _O_SHORT_LIVED;
    }
#endif

#ifdef _O_TEMPORARY
    if (strchr(flags, 'D'))  {
	*mode |= _O_TEMPORARY;
    }
#endif
#endif

    return 0;
}


/*
 * turn the file descriptor into a FILE*.  return NULL if fd == -1.
 * close the file descriptor if the fdopen fails.
 */
FILE* safe_fdopen(int fd, const char* flags)
{
    FILE* F;

    if (fd == -1)  {
	return NULL;
    }

    F = fdopen(fd, flags);
    if (!F)  {
	(void)close(fd);
    }

    return F;
}


/*
 * Wrapper functions that can be used as replacements for standard open and
 * fopen functions.  In C the initial permissions of the created file (mode)
 * will need to be added if missing.
 */




/* replacement function for fopen, fails if fn is a symbolic links */
FILE* safe_fopen_wrapper(const char *fn, const char *flags, mode_t mode)
{
    int create_file;
    int r;
    int open_flags;
    int f;

    /* let stdio_mode_to_open_flag return the error if flags is NULL */
    create_file = (flags != NULL && flags[0] != 'r');
    r = stdio_mode_to_open_flag(flags, &open_flags, create_file);

    if (r != 0) {
	return NULL;
    }

    f = safe_open_wrapper(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


/* replacement function for fopen, follows existing symbolic links */
FILE* safe_fopen_wrapper_follow(const char *fn, const char *flags, mode_t mode)
{
    int create_file;
    int r;
    int open_flags;
    int f;

    /* let stdio_mode_to_open_flag return the error if flags is NULL */
    create_file = (flags != NULL && flags[0] != 'r');
    r = stdio_mode_to_open_flag(flags, &open_flags, create_file);

    if (r != 0) {
	return NULL;
    }

    f = safe_open_wrapper_follow(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}