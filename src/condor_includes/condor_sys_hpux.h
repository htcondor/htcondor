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
#ifndef CONDOR_SYS_HPUX_H
#define CONDOR_SYS_HPUX_H

#define _XPG4_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#define _PROTOTYPES

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
#include <sys/vfs.h>
#include <sys/ustat.h>

/* Since both <sys/param.h> and <values.h> define MAXINT without
   checking, and we want to include both, we include them, but undef
   MAXINT inbetween. */
#include <sys/param.h>
#undef MAXINT
#include <values.h>

#if !defined(WCOREFLG)
#	define WCOREFLG 0x0200
#endif

#include <sys/fcntl.h>

#include "condor_hpux_64bit_types.h"

/* HPUX11 seems to have moved the exportfs() prototype elsewhere.... */
#if defined(HPUX10)
/* nfs/nfs.h is needed for fhandle_t.
   struct export is required to pacify prototypes
   of type export * before struct export is defined. */

struct export;
#endif
#include <nfs/nfs.h>

/* mount prototype */
#include <sys/mount.h>

/****************************************
** Condor-specific system definitions
****************************************/

#define HAS_64BIT_STRUCTS		1
#define SIGSET_CONST			const
#define HAS_U_TYPES			1
#define SYNC_RETURNS_VOID		1
#define SIGISMEMBER_IS_BROKEN		1

typedef void* MMAP_T;

#endif /* CONDOR_SYS_HPUX_H */

