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

#ifndef CONDOR_OPEN_H_INCLUDE
#define CONDOR_OPEN_H_INCLUDE

// needed for mode_t declaration
#include <sys/types.h>

// needed for FILE declaration, it is a typedef so an imcomplete struct will
// not work :(
#include <stdio.h>


#ifdef __cplusplus
extern "C"  {
#define SAFE_OPEN_DEFAULT_MODE = 0644
#else
#define SAFE_OPEN_DEFAULT_MODE
#endif

#ifndef SAFE_OPEN_RETRY_MAX
#define SAFE_OPEN_RETRY_MAX 50
#endif

/** @name Replacement functions for open.  These functions differ from the 
 standard C library open() in the following ways:
 <UL>
  <LI> file creation is always done safely and the semantics are determined by
    which of the 4 functions is used; also, a direct wrapper function is provided
	to make a reasonable default selection amongst the 4 functions.
  <LI> passing O_CREAT and O_EXCL to the safe_create* functions is optional
    (should not normally be passed)
  <LI> passing O_CREAT and O_EXCL to the safe_open_no_create is an error.
  <LI> file creation permissions have a default of 0644.  umask still applies.
    (should not pass unless something else is needed)
 </UL>
  All other flags are passed to open().
*/
//@{

	/// Typically you can call this as a direct wrapper for open().
int safe_open_wrapper(const char *fn, int flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file, error if exists, don't follow sym link.
int safe_create_fail_if_exists(const char *fn, int flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file, replace file if exists.  Use this instead of 
int safe_create_replace_if_exists(const char *fn, int flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file if it doesn't exist, keep inode if it does.  
int safe_create_keep_if_exists(const char *fn, int flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Open existing file.  
int safe_open_no_create(const char *fn, int flags);

//@}

/** @name Replacement functions for fopen().  These functions differ from the
 standard C library fopen() in the following ways:
 <UL>
  <LI> file creation is always done safely and the semantics are determined by
    which of the 4 functions is used; also, a direct wrapper function is provided
	to make a reasonable default selection amongst the 4 functions.
  <LI> the default file permission is 0644 instead of 0666.  umask still applies.
  <LI> an optional permissions argument is allowed
  <LI> in the case of "a", the file is opened with O_APPEND
 </UL>
*/
//@{

	/// Typically you can call this as a direct wrapper for fopen().
FILE* safe_fopen_wrapper(const char *fn, const char *flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file, fail if it exists, don't follow symlink.
FILE* safe_fcreate_fail_if_exists(const char *fn, const char* flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file, replace file if exists.
FILE* safe_fcreate_replace_if_exists(const char *fn, const char* flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Create file if it doesn't exist, keep inode if it does.
FILE* safe_fcreate_keep_if_exists(const char *fn, const char* flags, mode_t mode SAFE_OPEN_DEFAULT_MODE);

	/// Open existing file; fail if file does not exist.
FILE* safe_fopen_no_create(const char* fn, const char* flags);

//@}



#ifdef __cplusplus
}
#endif

#endif
