/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#if defined(GLIBC)
/* glibc defines the 3rd arg of readv and writev to be int, even
   though the man page (and our code) says it should be size_t. 
   -Derek Wright 4/17/98 */
#define readv __hide_readv
#define __readv ____hide_readv
#define writev __hide_writev
#define __writev ____hide_writev
#endif /* GLIBC */
#include <sys/uio.h>
#if defined(GLIBC)
#undef readv
#undef __readv
#undef writev
#undef __writev
BEGIN_C_DECLS
#if !defined(IA64)
int readv(int, const struct iovec *, size_t);
int __readv(int, const struct iovec *, size_t);
int writev(int, const struct iovec *, size_t);
int __writev(int, const struct iovec *, size_t);
#else
ssize_t readv(int, const struct iovec *, size_t);
ssize_t __readv(int, const struct iovec *, size_t);
ssize_t writev(int, const struct iovec *, size_t);
ssize_t __writev(int, const struct iovec *, size_t);
#endif
END_C_DECLS
#endif /* GLIBC */


#if defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))
#include <search.h>
#endif

/* swapon and swapoff prototypes */
#if !defined(IA64)
/* there is a bug in this header file for ia64 linux, figure out what I need
	out of here and prototype it manually. */
#include <linux/swap.h>
#endif

/* include stuff for malloc control */
#include <malloc.h>

/****************************************
** Condor-specific system definitions
***************************************/

#define HAS_U_TYPES	1
#define SIGSET_CONST	/* nothing */

#if defined(GLIBC22) || defined(GLIBC23)
	#define SYNC_RETURNS_VOID 1
#endif

/* typedef long long off64_t; */

typedef void* MMAP_T;

#endif /* CONDOR_SYS_LINUX_H */

