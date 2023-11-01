#ifndef SAFE_OPEN_H
#define SAFE_OPEN_H

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
#ifdef WIN32
typedef unsigned short mode_t;
#endif


#ifdef __cplusplus
extern "C"  {
#ifndef SAFE_OPEN_DEFAULT_MODE_VALUE
#define SAFE_OPEN_DEFAULT_MODE_VALUE 0600
#endif
#define SAFE_OPEN_DEFAULT_MODE = SAFE_OPEN_DEFAULT_MODE_VALUE
#else
#define SAFE_OPEN_DEFAULT_MODE
#endif

#ifndef SAFE_OPEN_RETRY_MAX
#define SAFE_OPEN_RETRY_MAX 50
#endif



/*
 * Replacement functions for open.  These functions differ in the following
 * ways:
 *
 * 1) file creation is always done safely and the semantics are determined by
 *    which of the 6 functions is used.
 * 2) passing O_CREAT and O_EXCL to the safe_create* functions is optional
 * 3) passing O_CREAT and O_EXCL to the safe_open_no_create is an error.
 * 4) file creation permissions have a default of SAFE_OPEN_DEFAULT_MODE_VALUE
 *    (0644 unless changed on compile line) in C++.  umask still applies.
 *    (should not pass unless something else is needed)
 * 5) symbolic links as the last component of the filename are never followed
 *    when creating a file
 * 6) symbolic links as the last component of the filename are never followed
 *    when opening an existing file, unless the *_follow versions are used
 * 7) all other flags are passed to open
 */

/* open existing file, do not follow final sym link */
int safe_open_no_create(const char *fn, int flags);
/* create file, error if it exists, don't follow symbolic links */
int safe_create_fail_if_exists(const char *fn, int flags,
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* create file, use existing file if it exists, do not follow final sym link */
int safe_create_keep_if_exists(const char *fn, int flags, 
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* create file, replacing the file if it exists */
int safe_create_replace_if_exists(const char *fn, int flags, 
				mode_t mode SAFE_OPEN_DEFAULT_MODE);
/* open existing file */
int safe_open_no_create_follow(const char *fn, int flags);
/* create file, use existing file if it exists */
int safe_create_keep_if_exists_follow(const char *fn, int flags, 
				mode_t mode SAFE_OPEN_DEFAULT_MODE);


/*
 * Wrapper functions for open/fopen replacements.  A simple replacement of the
 * existing function with these will result in symbolic links not being
 * followed and for a valid initial mode (file permissions) to always be
 * present.  In C the initial mode will have to be added where absent.
 */

/* safe wrapper to open, do not follow final sym link */
int safe_open_wrapper(const char *fn, int flags, 
				mode_t mode SAFE_OPEN_DEFAULT_MODE);

/* safe wrapper to open */
int safe_open_wrapper_follow(const char *fn, int flags, 
				mode_t mode SAFE_OPEN_DEFAULT_MODE);


/*
 * Functions to provide a path manipulation warning facility.
 *
 * If any of the open/create functions detect the file system object has
 * changed (removed, added or replaced) during their operation, the callback
 * function will be called with the file name.
 */

/* type of the callback function */
typedef int (*safe_open_path_warning_callback_type)(const char *fn);

/* registration function for path manipulation warning function.  Returns the
 * previous function.  NULL represents no registered callback
 */
safe_open_path_warning_callback_type safe_open_register_path_warning_callback(
					safe_open_path_warning_callback_type f
					);

/* Calling this function will call the callback function if registered */
int safe_open_path_warning(const char *fn);



#ifdef __cplusplus
}
#endif

extern int safe_open_last_fd;

#endif
