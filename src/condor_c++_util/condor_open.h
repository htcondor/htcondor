/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_OPEN_H_INCLUDE
#define CONDOR_OPEN_H_INCLUDE

// needed for mode_t declaration
#include <sys/types.h>

// needed for FILE declaration, it is a typedef so an imcomplete struct will
// not work :(
#include <stdio.h>


/** @name Replacement functions for open.  These functions differ from the 
 standard C library open() in the following ways:
 <UL>
  <LI> file creation is always done safely and the semantics are determined by
    which of the 4 functions is used; also, a direct wrapper function is provided
	to make a reasonable default selection amongst the 4 functions.
  <LI> passing O_CREAT and O_EXCL to the condor_create* functions is optional
    (should not normally be passed)
  <LI> passing O_CREAT and O_EXCL to the condor_open_no_create is an error.
  <LI> file creation permissions have a default of 0644.  umask still applies.
    (should not pass unless something else is needed)
 </UL>
  All other flags are passed to open().
*/
//@{

	/// Typically you can call this as a direct wrapper for open().
int open_safely_wrapper(const char *fn, int flags, mode_t mode = 0644);

	/// Create file, error if exists, don't follow sym link.
int open_create_fail_if_exists(const char *fn, int flags, mode_t mode = 0644);

	/// Create file, replace file if exists.  Use this instead of 
int open_create_replace_if_exists(const char *fn, int flags, mode_t mode = 0644);

	/// Create file if it doesn't exist, keep inode if it does.  
int open_create_keep_if_exists(const char *fn, int flags, mode_t mode = 0644);

	/// Open existing file.  
int open_no_create(const char *fn, int flags);

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
FILE* fopen_safely_wrapper(const char *fn, const char *flags, mode_t mode = 0644);

	/// Create file, fail if it exists, don't follow symlink.
FILE* fopen_create_fail_if_exists(const char *fn, const char* flags, mode_t mode = 0644);

	/// Create file, replace file if exists.
FILE* fopen_create_replace_if_exists(const char *fn, const char* flags, mode_t mode = 0644);

	/// Create file if it doesn't exist, keep inode if it does.
FILE* fopen_create_keep_if_exists(const char *fn, const char* flags, mode_t mode = 0644);

	/// Open existing file; fail if file does not exist.
FILE* fopen_no_create(const char* fn, const char* flags);

//@}

#endif
