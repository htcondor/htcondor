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

/* nfs/nfs.h is needed for fhandle_t.
   struct export is required to pacify prototypes
   of type export * before struct export is defined. */

struct export;
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

