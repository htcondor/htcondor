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

 

#if defined(AIX31) || defined(AIX32)
#include "syscall.aix.h"
#endif

#if defined(IRIX331)
#include <sys.s>
#elif defined(IRIX53)
#	undef SYSVoffset
#	undef __SYS_S__
#	include <sys.s>
#endif

/* Solaris specific change ..dhaval 6/23
*/
#if defined(Solaris)
#include <sys/syscall.h>
#endif

#if !defined(AIX31) && !defined(AIX32)  && !defined(IRIX331) && !defined(Solaris) && !defined(IRIX53) && !defined(WIN32)
#include <syscall.h> 
#endif

#if defined(AIX31) || defined(AIX32)
extern int	CondorErrno;
#define SET_ERRNO errno = CondorErrno
#else
#define SET_ERRNO
#endif

extern int	errno;

/*
**	System calls can be either:
**		local or remote...
**		recorded or unrecorded...
*/

#define SYS_LOCAL		0x0001		/* Perform local system calls            */
#define SYS_REMOTE		0			/* Comment for calls to SetSyscalls     */
#define SYS_RECORDED	0x0002		/* Record information needed for restart */
#define SYS_UNRECORDED	0			/* Comment for calls to SetSyscalls     */
#define SYS_MAPPED		SYS_RECORDED    /* more descriptive name */
#define SYS_UNMAPPED	SYS_UNRECORDED   /* more descriptive name */

/*
extern int Syscalls;
*/
#if defined(__cplusplus)
extern "C" {
#endif
extern char *CONDOR_SyscallNames[];
extern char *_condor_syscall_name(int);
#if defined(__cplusplus)
}
#endif

#include "syscall_numbers.h"

#define NFAKESYSCALLS    22

#define NREALSYSCALLS	 242
#define CONDOR_SYSCALLNAME(sysnum) \
			((((sysnum) >= -NFAKESYSCALLS) && (sysnum <= NREALSYSCALLS)) ? \
				CONDOR_SyscallNames[(sysnum)+NFAKESYSCALLS] : \
				"Unknown CONDOR system call number" )


#define CONDOR__load	CONDOR_load
