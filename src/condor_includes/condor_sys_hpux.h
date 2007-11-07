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

/* no seteuid() on HPUX11: we define our own which wraps setreuid() */
#if defined(HPUX11)
#if defined(__cplusplus)
extern "C" int seteuid(uid_t);
extern "C" int setegid(gid_t);
#else
int seteuid(uid_t);
int setegid(gid_t);
#endif
#endif

#endif /* CONDOR_SYS_HPUX_H */

