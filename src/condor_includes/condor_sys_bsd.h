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
#ifndef CONDOR_SYS_BSD_H
#define CONDOR_SYS_BSD_H

/*#define _XPG4_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#define _PROTOTYPES */

#if defined(_POSIX_SOURCE)
#    undef  _POSIX_SOURCE
#endif

#include <sys/types.h>

#include <unistd.h>
/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>
#include <stdio.h>

/* There is no <sys/select.h> on HPUX, select() and friends are 
   defined in <sys/time.h> */
#include <sys/time.h>

/* Need these to get statfs and friends defined */
#include <sys/stat.h>

/* Since both <sys/param.h> and <values.h> define MAXINT without
   checking, and we want to include both, we include them, but undef
   MAXINT inbetween. */
#include <sys/param.h>

#if !defined(WCOREFLG)
#	define WCOREFLG 0x0200
#endif

#include <sys/fcntl.h>


/* nfs/nfs.h is needed for fhandle_t.
   struct export is required to pacify prototypes
   of type export * before struct export is defined. */


/* mount prototype */
#include <sys/mount.h>

/* for DBL_MAX */
#include <float.h>
/* Darwin does not define a SYS_NMLN, but rather calls it __SYS_NAMELEN */

#define SYS_NMLN  _SYS_NAMELEN
/****************************************
** Condor-specific system definitions
****************************************/

#define HAS_64BIT_STRUCTS		0
#define SIGSET_CONST			const
#define SYNC_RETURNS_VOID		1
#define HAS_U_TYPES			1
#define SIGISMEMBER_IS_BROKEN		1

#define MAXINT				INT_MAX
typedef void* MMAP_T;

#endif /* CONDOR_SYS_BSD_H */

