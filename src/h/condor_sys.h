/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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
extern char *CONDOR_SyscallNames[];

#if !defined(WIN32)
#include "syscall_numbers.h"
#endif

#define NFAKESYSCALLS    22

#define NREALSYSCALLS	 242
#define CONDOR_SYSCALLNAME(sysnum) \
			((((sysnum) >= -NFAKESYSCALLS) && (sysnum <= NREALSYSCALLS)) ? \
				CONDOR_SyscallNames[(sysnum)+NFAKESYSCALLS] : \
				"Unknown CONDOR system call number" )


#define CONDOR__load	CONDOR_load
