/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef CONDOR_SYSCALL_SYSDEP_H
#define CONDOR_SYSCALL_SYSDEP_H

#if defined(LINUX)
#   if defined(GLIBC) 
#		if defined(GLIBC21)
#			define MMAP_T void*
			/* The man page on a glibc21 machine is wrong, there is no const */
#			define GLIBC_CONST 
#		elif defined(GLIBC20)
#			define MMAP_T char*
#			define GLIBC_CONST 
#		else
#			die "You need to define MMAP_T and GLIBC_CONST"
#		endif
#   else
#		define MMAP_T void*
#		define GLIBC_CONST const
#   endif
#endif

/* this is used for ftruncate */
#if defined(LINUX)
#	if defined(GLIBC21)
#		define LENGTH_TYPE off_t
#	else
#		define LENGTH_TYPE size_t
#	endif
#endif

#if defined(OSF1)
#   define SYSCALL_PTR	void
#   define INV_SYSCALL_PTR	char
#   define NEED_CONST	const
#elif defined(HPUX)
#   define SYSCALL_PTR	char
#   define INV_SYSCALL_PTR	const void
#	define NEED_CONST
#elif defined(Solaris27)
#   define SYSCALL_PTR	void
#   define INV_SYSCALL_PTR void
#	define NEED_CONST const
#else
#   define SYSCALL_PTR	char
#   define INV_SYSCALL_PTR	void
#   if defined(Solaris26)
#	define NEED_CONST	const
#   else
#	define NEED_CONST
#   endif
#endif

#if defined(LINUX)
#	if defined(GLIBC22) || defined(GLIBC23)
#		define SYNC_RETURNS_VOID 1
#	else
#		define SYNC_RETURNS_VOID 0
#	endif
#else /* Solaris, IRIX, Alpha, DUX4.0 all confirmed... */
#	define SYNC_RETURNS_VOID 1
#endif

#if defined(HPUX10) || defined(Solaris)
#	define HAS_64BIT_STRUCTS	1
#	define HAS_64BIT_SYSCALLS	1
#endif

#if defined(Solaris27)
#   define SOCKLEN_PTR Psocklen_t
#	define SOCKLEN_T socklen_t
#else
#   define SOCKLEN_PTR int *
#	define SOCKLEN_T int
#endif

#endif /* CONDOR_SYSCALL_SYSDEP_H */
