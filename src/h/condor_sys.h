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


 

#if defined(IRIX)
#	undef SYSVoffset
#	undef __SYS_S__
#	include <sys.s>
#endif

/* Solaris specific change ..dhaval 6/23
*/
#if defined(Solaris)
#include <sys/syscall.h>
#endif

#if defined(HPUX) || defined(LINUX) || defined(DUX)
#include <syscall.h> 
#endif

#if defined (LINUX) && !defined(SYS_sigsetmask)

/* SYS_sigsetmask is undefined in glibc header files, however it is a valid
	system call in linux kernels so to write portable(stop laughing) code
	we want it to be defined in both cases. It turns out that while this exists
	in i386 kernel, it simply doesn't in x86_86 kernels. */
#if defined(I386)
#define SYS_sigsetmask __NR_ssetmask
#endif

#endif

#if defined (LINUX)
/* Under 2.2 kernels SYS_chown isn't the same as SYS_chown on a 2.0 kernel.
	From the man page for chown...

       In versions of Linux prior to 2.1.81  (and  distinct  from
       2.1.46), chown did not follow symbolic links.  Since Linux
       2.1.81, chown does follow symbolic links, and there  is  a
       new  system  call  lchown  that  does  not follow symbolic
       links.  Since Linux 2.1.86, this new call  (that  has  the
       same  semantics as the old chown) has got the same syscall
       number, and chown got the newly introduced number.

	So what we are going to do is to make SYS_chown act like SYS_lchown
	so that programs compiled under a 2.2 kernel headerfile system can
	still run under a 2.0 machine.

	-pete 8/15/99
*/
#	if defined(SYS_lchown)
#		undef SYS_chown
#		define SYS_chown SYS_lchown
#	endif
#endif

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
