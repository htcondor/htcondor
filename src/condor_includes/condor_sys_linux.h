/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_SYS_LINUX_H
#define CONDOR_SYS_LINUX_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#if defined(GLIBC)
#define _GNU_SOURCE
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
int readv(int, const struct iovec *, size_t);
int __readv(int, const struct iovec *, size_t);
int writev(int, const struct iovec *, size_t);
int __writev(int, const struct iovec *, size_t);
END_C_DECLS
#endif /* GLIBC */


#if defined(LINUX) && defined(GLIBC21)
#include <search.h>
#endif

/* swapon and swapoff prototypes */
#include <linux/swap.h>

/****************************************
** Condor-specific system definitions
***************************************/

#define HAS_U_TYPES	1
#define SIGSET_CONST	/* nothing */

/* typedef long long off64_t; */
typedef void* MMAP_T;

#endif /* CONDOR_SYS_LINUX_H */

