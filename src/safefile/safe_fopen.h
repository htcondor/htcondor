#ifndef SAFE_FOPEN_H
#define SAFE_FOPEN_H

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



/* needed for mode_t declaration */
#include <sys/types.h>


/* needed for FILE declaration, it is a typedef so an incomplete struct will
 * not work :(
 */
#include <stdio.h>

#ifdef __cplusplus
extern "C"  {
#ifndef SAFE_OPEN_DEFAULT_MODE_VALUE
#define SAFE_OPEN_DEFAULT_MODE_VALUE 0600
#endif
#define SAFE_OPEN_DEFAULT_MODE = SAFE_OPEN_DEFAULT_MODE_VALUE
#else
#define SAFE_OPEN_DEFAULT_MODE
#endif
#ifdef WIN32
typedef unsigned short mode_t;
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


/* create file, error if it exists, don't follow symbolic links */
FILE* safe_fcreate_fail_if_exists(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* create file, replacing the file if it exists */
FILE* safe_fcreate_replace_if_exists(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* create file, use existing file if it exists, do not follow final sym link */
FILE* safe_fcreate_keep_if_exists(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* open existing file, do not follow final sym link */
FILE* safe_fopen_no_create(const char* fn, const char* flags);
/* create file, use existing file if it exists */
FILE* safe_fcreate_keep_if_exists_follow(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* open existing file */
FILE* safe_fopen_no_create_follow(const char* fn, const char* flags);


/*
 * Wrapper functions for open/fopen replacements.  A simple replacement of the
 * existing function with these will result in symbolic links not being
 * followed and for a valid initial mode (file permissions) to always be
 * present.  In C the initial mode will have to be added.
 */

/* safe wrapper to fopen, do not follow final sym link */
FILE* safe_fopen_wrapper(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);

/* safe wrapper to fopen */
FILE* safe_fopen_wrapper_follow(const char *fn, const char* flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);


#ifdef __cplusplus
}
#endif

#endif
