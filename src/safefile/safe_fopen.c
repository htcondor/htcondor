/*
 * safefile package    http://www.cs.wisc.edu/~kupsch/safefile
 *
 * Copyright 2007-2008, 2011 James A. Kupsch
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
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>
#include <stdio.h>
#include "safe_fopen.h"
#include "safe_open.h"




/*
 * Internal helper function declarations
 */
static int stdio_mode_to_open_flag(const char *flags, int *mode, int create_file);
static FILE *safe_fdopen(int fd, const char *flags);

#ifdef WIN32
#include <string.h>
#include <stdlib.h>
#include <io.h>
static char *fix_stdio_fdopen_mode_on_windows(const char *flags);

const char ccsStr[] = "ccs=";
const char unicodeStr[] = "UNICODE";
const char utf8Str[] = "UTF-8";
const char utf16Str[] = "UTF-16LE";

const size_t ccsStrLen = sizeof(ccsStr) - 1;
const size_t unicodeStrLen = sizeof(unicodeStr) - 1;
const size_t utf8StrLen = sizeof(utf8Str) - 1;
const size_t utf16StrLen = sizeof(utf16Str) - 1;
#endif


/*
 * Replacement functions for fopen.  These functions differ in the following
 * ways:
 *
 * 1) file creation is always done safely with file creation semantics determined
 *    by the choice of the 4 available functions, allowing semantics not possible
 *    using fopen
 * 2) the default file permission is 0644 instead of 0666.  umask still applies.
 * 3) a file permissions argument exists for file creation (optional in C++)
 * 4) in the case of flags containing "a", the file is opened with O_APPEND
 * 5) EINVAL is returned if the filename or flags are a NULL pointer, or the
 *    first character of the flag is not a valid standard fopen flag value:  rwa
 * 6) EEXISTS is returned if the final component is a symbolic link in both
 *    the case of creating and of opening an existing file
 */


/* open existing file */
FILE *safe_fopen_no_create(const char *fn, const char *flags)
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
FILE *safe_fcreate_fail_if_exists(const char *fn, const char *flags, mode_t mode)
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
FILE *safe_fcreate_keep_if_exists(const char *fn, const char *flags, mode_t mode)
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
FILE *safe_fcreate_replace_if_exists(const char *fn, const char *flags, mode_t mode)
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
FILE *safe_fopen_no_create_follow(const char *fn, const char *flags)
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
FILE *safe_fcreate_keep_if_exists_follow(const char *fn, const char *flags, mode_t mode)
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
 * Helper functions for use in safe_fcreate and safe_fopen function families
 */

/*
 * Create the flags needed by open from mode string passed to fopen.
 * Fail if the create_file is true and mode is "r" or "rb", or if the
 * mode does not start with one of the 15 valid modes:
 *   r rb r+ r+b rb+  w wb w+ w+b wb+  a ab a+ a+b ab+
 * Also fail if the flags or mode pointers are NULL.
 */
int stdio_mode_to_open_flag(const char *flags, int *mode, int create_file)
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

#ifdef WIN32
    /* deal with extra flags on windows:
     *     b t S R T D N ccs=UNICODE ccs=UTF8 ccs=UTF16LE
     */
    {
	const char *f = &flags[1];

	/* This loop skips the first character, but that is what we want as the
	 * first character is always 'r', 'w', or 'a', and we have already 
	 * processed them.
	 */
	while (*f)  {
	    switch (*f)  {
	    case '+':
		/* Ignore. Handled above. */
		break;

	    case 'b':
#ifdef _O_BINARY
		*mode |= _O_BINARY;
#else
		/* b mode is always valid, even if _O_BINARY is not defined */
#endif
		break;

#ifdef _O_TEXT
	    case 't':
		*mode |= _O_TEXT;
		break;
#endif

#ifdef _O_SEQUENTIAL
	    case 'S':
		*mode |= _O_SEQUENTIAL;
		break;
#endif

#ifdef _O_RANDOM
	    case 'R':
		*mode |= _O_RANDOM;
		break;
#endif

#ifdef _O_SHORT_LIVED
	    case 'T':
		*mode |= _O_SHORT_LIVED;
		break;
#endif

#ifdef _O_TEMPORARY
	    case 'D':
		*mode |= _O_TEMPORARY;
		break;
#endif

#ifdef _O_NOINHERIT
	    case 'N':
		*mode |= _O_NOINHERIT;
		break;
#endif

	    case 'n':
		/* found plain 'n', no impact on mode */
		break;

	    case 'c':
#if defined(_O_WTEXT) || defined(_O_UTF8) || defined(_O_UTF16)
		if (strncmp(f, ccsStr, ccsStrLen) != 0)  {
#endif
		    /* found plain 'c', no impact on mode */
		    break;
#if defined(_O_WTEXT) || defined(_O_UTF8) || defined(_O_UTF16)
		}  else  {
		    /* found ccs= sequence */
		    f += ccsStrLen;
#ifdef _O_WTEXT
		    if (strncmp(f, unicodeStr, unicodeStrLen) == 0)  {
			/* found ccs=UNICODE sequence */
			f += unicodeStrLen - 1;
			*mode |= _O_WTEXT;
			break;
		    }
#endif
#ifdef _O_UTF8
		    if (strncmp(f, utf8Str, utf8StrLen) == 0)  {
			/* found ccs=UTF8 sequence */
			f += utf8StrLen - 1;
			*mode |= _O_UTF8;
			break;
		    }
#endif
#ifdef _O_UTF16
		    if (strncmp(f, utf16Str, utf16StrLen) == 0)  {
			/* found ccs=UTF16LE sequence */
			f += utf16StrLen - 1;
			*mode |= _O_UTF16;
			break;
		    }
#endif
		    /* no valid ccs sequence found */
		    errno = EINVAL;
		    return -1;
		}
		/* no valid ccs sequence found */
		errno = EINVAL;
		return -1;
#endif

	    case ',':
		/* Ignore commas.  Used to separate modes like ccs=ENCODING */
		break;

	    default:
		errno = EINVAL;
		return -1;
	    }

	++f;
	}
    }
#endif

    return 0;
}


#ifdef WIN32
/*
 * fdopen on windows does not accept the N flag in the mode string.
 * Create a new mode string without the N flag
 */
char *fix_stdio_fdopen_mode_on_windows(const char *flags)
{
    const char *from;
    char *new_flags;
    char *to;

    if (flags == NULL)  {
	errno = EINVAL;
	return NULL;
    }

    new_flags = malloc(strlen(flags) + 1);
    if (new_flags == NULL)  {
	return NULL;
    }

    from = flags;
    to = new_flags;

    do  {
	switch (*from)  {
	case 'N':
	    /* N isn't allowed in fdopen, so skip. */
	    ++from;
	    break;

	case 'c':
#if defined(_O_WTEXT) || defined(_O_UTF8) || defined(_O_UTF16)
	    if (strncmp(from, ccsStr, ccsStrLen) != 0)  {
#endif
		/* found 'c' not part of ccs, copy it */
		*to++ = *from++;
		break;
#if defined(_O_WTEXT) || defined(_O_UTF8) || defined(_O_UTF16)
	    }  else  {
		/* found ccs= sequence */
		memcpy(to, ccsStr, ccsStrLen);
		from += ccsStrLen;
		to += ccsStrLen;
#ifdef _O_WTEXT
		if (strncmp(from, unicodeStr, unicodeStrLen) == 0)  {
		    /* found ccs=UNICODE sequence */
		    memcpy(to, unicodeStr, unicodeStrLen);
		    from += unicodeStrLen;
		    to += unicodeStrLen;
		}
#endif
#ifdef _O_UTF8
		if (strncmp(from, utf8Str, utf8StrLen) == 0)  {
		    /* found ccs=UTF8 sequence */
		    memcpy(to, utf8Str, utf8StrLen);
		    from += utf8StrLen;
		    to += utf8StrLen;
		    break;
		}
#endif
#ifdef _O_UTF16
		if (strncmp(from, utf16Str, utf16StrLen) == 0)  {
		    /* found ccs=UTF16LE sequence */
		    memcpy(to, utf16Str, utf16StrLen);
		    from += utf16StrLen;
		    to += utf16StrLen;
		    break;
		}
#endif
		/* no valid ccs sequence found */
		errno = EINVAL;
		free(new_flags);
		return NULL;
	    }
#endif
	default:
	    /* handle: + b t n S R T D , */
	    *to++ = *from++;
	    break;
	}
    }  while (*from);

    /* null terminate the new flags string */
    *to = 0;

    return new_flags;
}
#endif


/*
 * Turn the file descriptor into a FILE*.  Return NULL if fd == -1.
 * close the file descriptor if the fdopen fails.
 */
FILE *safe_fdopen(int fd, const char *flags)
{
    FILE *F;
#ifdef WIN32
    char *new_flags;
#endif

    if (fd == -1)  {
	return NULL;
    }

#ifdef WIN32
    new_flags = fix_stdio_fdopen_mode_on_windows(flags);
    if (new_flags == NULL)  {
	(void)close(fd);
	errno = EINVAL;
	return NULL;
    }
    flags = new_flags;
#endif

    F = fdopen(fd, flags);
    if (!F)  {
	(void)close(fd);
    }

#ifdef WIN32
    free(new_flags);
#endif

    return F;
}


/*
 * Wrapper functions that can be used as replacements for standard open and
 * fopen functions.  In C the initial permissions of the created file (mode)
 * will need to be added if missing.
 */




/* replacement function for fopen, fails if fn is a symbolic links */
FILE *safe_fopen_wrapper(const char *fn, const char *flags, mode_t mode)
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
FILE *safe_fopen_wrapper_follow(const char *fn, const char *flags, mode_t mode)
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
