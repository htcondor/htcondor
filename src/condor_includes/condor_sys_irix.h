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

#ifndef CONDOR_SYS_IRIX_H
#define CONDOR_SYS_IRIX_H


#if defined(IRIX65)

/* On IRIX 6.5 the header files are just generally wacky.  So lets
   define what we want.  NOTE: things like _ABIAPI only work in #if
   statments! */

/* The following should set things like _ABIAPI to something that
we want, and it gives us a lot of the 64 bit stuff that was lacking
before */

#define _SGI_SOURCE 1
#define _ABI_SOURCE 1

/* We also want LARGEFILE support so define: */

#define _LARGEFILE64_SOURCE 1

/* We also can do without _POSIX_SOURCE on IRIX */
#if defined(_POSIX_SOURCE)
#	undef _POSIX_SOURCE
#endif

#endif

#if defined(IRIX62)
#	define _XOPEN_SOURCE 1
#	define _XOPEN_SOURCE_EXTENDED 1
#endif

#define _BSD_COMPAT 1

/* While we want _BSD_TYPES defined, we can't just define it ourself,
   since we include rpc/types.h later, and that defines _BSD_TYPES
   itself without any checks.  So, instead, we'll just include
   <rpc/types.h> as our first header, to make sure we get BSD_TYPES.
   This also includes <sys/types.h>, so we don't need to include that
   ourselves anymore. */

#include <rpc/types.h>

#if defined(IRIX65)
/* Now time for some sanity checks... */
#if !(_ABIAPI)
#	error "STOP:  Check out why _ABIAPI is FALSE!"
#endif

#endif


#if defined(IRIX65)
#	undef _ABIAPI
#	include <sys/resource.h>
#	define _ABIAPI 1
#endif /* defined (IRIX65) */

/* For strdup to get prototyped, we need to still have the XOPEN stuff 
   defined when we include string.h */ 
/* XXX Need to check this */
#include <string.h>

/******************************
** unistd.h
******************************/
#if defined(IRIX62)
#	define __vfork fork
#endif

/* if _BSD_COMPAT is defined, then getpgrp will get funny arguments from 
   <unistd.h> and that will break receivers.c - Ballard 10/99
   ( and note how this is done! )

   Also, <grp.h> gives us funny arguments for initgroups() if this is
   turned on, so we'll include that now, too. - Derek 2004-01-23
*/
#if _BSD_COMPAT
#	define _save_BSD_COMPAT 1
#else
#	define _save_BSD_COMPAT 0
#endif
#undef _BSD_COMPAT

#include <unistd.h>
#include <grp.h>
#if defined(IRIX62)
#	undef __vfork
#endif
#define _BSD_COMPAT _save_BSD_COMPAT
#undef _save_BSD_COMPAT

/* Some prototypes we should have but did not pick up from unistd.h */
BEGIN_C_DECLS
extern int getpagesize(void);
END_C_DECLS

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/******************************
** stdio.h
******************************/
#include <stdio.h>
/* These aren't defined if _POSIX_SOURCE is defined. */
FILE *popen (const char *command, const char *type);
int pclose(FILE *stream);


/******************************
** signal.h
******************************/
#define SIGTRAP 5
#define SIGBUS 10
#define SIGIO 22

/* XXX fix me */
#define _save_NO_ANSIMODE _NO_ANSIMODE
#undef _NO_ANSIMODE
#define _NO_ANSIMODE 1

#if defined(IRIX62)
#	undef _SGIAPI
#	define _SGIAPI 1
#endif

#include <signal.h>
#include <sys/signal.h>
/* We also want _NO_ANSIMODE and _SGIAPI defined to 1 for sys/wait.h,
   math.h and limits.h */  
#include <sys/wait.h>    
/* Some versions of IRIX don't define WCOREFLG, but we need it */
#if !defined( WCOREFLG )
#  define WCOREFLG 0200
#endif

#include <math.h>
#include <limits.h>
#undef _NO_ANSIMODE
#define _NO_ANSIMODE _save_NO_ANSIMODE
#undef _save_NO_ANSIMODE

/******************************
** sys/socket.h
******************************/
/* On Irix 6.5, they actually implement socket() in sys/socket.h by
   having it call some other xpg socket thing, if you're using XOPEN4.
   We can't have that, or file_state.C gets really confused.  So, if
   we're building the ckpt_lib, turn off XOPEN4.
	!!!THIS IS BAAAAD. We are just hacking the _NO_XOPEN4 since we
	know what the value is. This should be fixed to correctly hold
	onto the value and restore it afterward.

* #if defined(IRIX65) && defined(IN_CKPT_LIB)
* #undef _NO_XOPEN4
* #define _NO_XOPEN4 1
* #include <sys/socket.h>
* #undef _NO_XOPEN4
* #define _NO_XOPEN4 0
* #endif

	-- NOTE.  You cannot do the following:

	# define _save_foo foo
	# define foo 1
	....

	#define foo _save_foo

*/



#include <sys/select.h>

/* A bunch of sanity checks. */
#if defined(IRIX65)

#if !(_LFAPI)
#	error "STOP:  Find out why _LFAPI is false!!!"
#endif
#if !(_SGI_SOURCE)
#	error "STOP:  Find out why _SGIAPI is false!!!"
#endif

#if !(_NO_POSIX)
#	error "STOP:  Find out why _NO_POSIX is false!!!"
#endif

#if (!_NO_XOPEN4)
#	error "STOP:  Find out why _NO_XOPEN4 is false!!!"
#endif

#if (!_NO_XOPEN5)
#	error "STOP:  Find out why _NO_XOPEN5 is false!!!"
#endif

#if !(_SGIAPI)
#	error "STOP:  Find out why _SGIAPI is false!!!"
#endif

#if defined(_POSIX_SOURCE)
#	error "STOP:  why is _POSIX_SOURCE defined?"
#endif

#if defined(_POSIX_C_SOURCE)
#	error "STOP:  why is _POSIX_C_SOURCE defined?"
#endif

#endif /* IRIX65 */


#undef _SGIAPI
#define _SGIAPI 1

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/statfs.h>

/* need some dirent support */
#include <dirent.h>

#if defined(IRIX62)
/* Needed to get TIOCNOTTY defined - not needed after IRIX62 */
#include <sys/ttold.h>
#endif

/****************************************
** Condor-specific system definitions
****************************************/

#if defined(IRIX65)
#	include <sys/file.h>

/* Need these so we can use sysget() */
#	include<inttypes.h>
#	include<sys/types.h>
#	include<sys/cellkernstats.h>
#	include<sys/sysget.h>
#endif

#define HAS_64BIT_SYSCALLS 1


#define HAS_U_TYPES			1
#define SIGSET_CONST			const
#define SYNC_RETURNS_VOID		1
#define NO_VOID_SIGNAL_RETURN		1
#define HAS_64BIT_STRUCTS		1

typedef void * MMAP_T;

#endif /* CONDOR_SYS_IRIX_H */
