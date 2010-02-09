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
#else /* Solaris, Alpha, DUX4.0 all confirmed... */
#	define SYNC_RETURNS_VOID 1
#endif

#if defined(Solaris)
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
