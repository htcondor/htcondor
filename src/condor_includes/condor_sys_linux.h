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

#ifndef CONDOR_SYS_LINUX_H
#define CONDOR_SYS_LINUX_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#if defined(GLIBC)
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

#if defined(GLIBC23)
/* the glibc in redhat 9, and the fortran compiler in particular use 
	the 64 bit interfaces that we have to worry about */
/*#define _LARGEFILE64_SOURCE*/
/*#define _FILE_OFFSET_BITS 64*/
/*#define HAS_64BIT_SYSCALLS      1*/
/*#define HAS_64BIT_STRUCTS      1*/
#endif

#include <sys/types.h>

#include "condor_fix_sys_stat.h"
#include "condor_fix_unistd.h"

/* Allows use of setgroups() */
/* It turns out we can't allow this inclusion to fix some other warnings
	in the source code concerning setgroups() since it messes up some 
	hand specified prototypes in the standard universe codebase. Once that
	is investigated and fixed, this can be included properly. */
/*#include <grp.h>*/

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/* <stdio.h> on glibc Linux defines a "dprintf()" function, which
   we've got hide since we've got our own. */
#if defined(GLIBC)
#	define dprintf _hide_dprintf
#	define getline _hide_getline
#endif
#include <stdio.h>
#if defined(GLIBC)
#	undef dprintf
#	undef getline
#endif

#define SignalHandler _hide_SignalHandler
#include <signal.h>
#undef SignalHandler

/* Since param.h defines NBBY  w/o check, we want to include it here 
   before we check for and define it ourselves */ 
#include <sys/param.h>

/* There is no <sys/select.h> on Linux, select() and friends are 
   defined in <sys/time.h> */
#include "condor_fix_sys_time.h"

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/vfs.h>

/* This headerfile must be included before sys/wait.h because on glibc22
	machines sys/wait.h brings in sys/resource.h and we need to modify
	a few prototypes in sys/resource.h to be POSIX compliant.
	-psilord 4/25/2001 */
#include "condor_fix_sys_resource.h"
#include <sys/wait.h>
/* Some versions of Linux don't define WCOREFLG, but we need it */
#if !defined( WCOREFLG )
#	define WCOREFLG 0200
#endif 

#if !defined(GLIBC20) && !defined(GLIBC21)
#include <search.h>
#endif

/* swapon and swapoff prototypes */
#if !defined(IA64) && !defined(CONDOR_PPC) && !defined(LINUX)
/* there is a bug in this header file for ia64 linux, figure out what I need
	out of here and prototype it manually. */
#include <linux/swap.h>
#endif

/* include stuff for malloc control */
#include <malloc.h>

/* to get the sysinfo() function call */
#include <sys/sysinfo.h>

#if defined( I386 ) || defined( X86_64 )
/* For the i386 execution domains for standard
	universe executables. Under redhat 9 and later there is a
	sys/personality.h we could include here as well so we can use
	the personality() function to change execution domains to get
	good memory map layout behavior. However, before redhat 9 that
	header file and function call don't exist. The system call
	itself does exist, however, so we'll just be calling syscall()
	with the right system call symbol to get the job done on all of the
	distributions. */

#include <sys/syscall.h>

/* Here is where we find the PER_* constants for the personality system call. */
#if HAVE_LINUX_PERSONALITY_H
/* Warning: this defines a lexical replacement for 'personality()'
	which is not a function call */ 
#include <linux/personality.h>
#elif HAVE_SYS_PERSONALITY_H
/* this defines a true function called 'personality()' which changes the
	execution domain of the process */
#include <sys/personality.h>
#endif

#endif

/****************************************
** Condor-specific system definitions
***************************************/

#define HAS_U_TYPES	1
#define SIGSET_CONST	/* nothing */

#if !defined(GLIBC20) && !defined(GLIBC21)
	#define SYNC_RETURNS_VOID 1
#endif

/* typedef long long off64_t; */

typedef void* MMAP_T;

#endif /* CONDOR_SYS_LINUX_H */

