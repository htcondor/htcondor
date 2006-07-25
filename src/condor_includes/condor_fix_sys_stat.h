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

#ifndef _CONDOR_FIX_SYS_STAT_H
#define _CONDOR_FIX_SYS_STAT_H

/*
On many platforms, stat defines several static functions
which redirect stat to xstat, fstat to fxstat, and so on.
We want to kill all of these definitions right here
and then provide prototypes for the libc versions
of stat and friends.

The whole point of this is to enable us to redefine
the libc versions of stat and friends as we please.
We only perform this action when CONDOR_KILL_STAT_DEFINITIONS
is in effect.  This should probably only be set
by the syscall library.

Notice that a #define stat is not what we want -- this
will clobber all sorts of arguments and structures that
use the symbol stat.  By defining a macros with arguments,
stat(), we will only kill the static function definition.
*/

#include "condor_header_features.h"

#if defined(CONDOR_KILLS_STAT_DEFINITIONS)
	#define mknod(a,b,c) __condor_hack_mknod(a,b,c)
	#define stat(a,b) __condor_hack_stat(a,b)
	#define lstat(a,b) __condor_hack_lstat(a,b)
	#define fstat(a,b) __condor_hack_fstat(a,b)
	#define __mknod(a,b,c) __condor_hack___mknod(a,b,c)
	#define __stat(a,b) __condor_hack___stat(a,b)
	#define __lstat(a,b) __condor_hack___lstat(a,b)
	#define __fstat(a,b) __condor_hack___fstat(a,b)
#endif

#include <sys/stat.h>

#if defined(CONDOR_KILLS_STAT_DEFINITIONS)
	#undef fstat
	#undef lstat
	#undef stat
	#undef mknod
	#undef __fstat
	#undef __lstat
	#undef __stat
	#undef __mknod

	/* Now, we must provide the protoypes that we lost. */

	BEGIN_C_DECLS
	extern int stat(const char *path, struct stat *buf);
	extern int lstat(const char *path, struct stat *buf);
	extern int fstat(int fd, struct stat *buf);
	extern int mknod(const char *path, mode_t mode, dev_t device);
	END_C_DECLS
#endif

#endif /* _CONDOR_FIX_SYS_STAT_H */
