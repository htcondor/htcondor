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

#ifndef CONDOR_SYS_SOLARIS_H
#define CONDOR_SYS_SOLARIS_H

/* This allows things like gethostname()'s definition to be brought in */
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29) || defined(Solaris10) || defined(Solaris11)
#ifndef _LARGEFILE64_SOURCE
#	define _LARGEFILE64_SOURCE
#endif
#endif

#include "condor_fix_sys_utsname.h"
#include "condor_fix_sys_stat.h"

#include <sys/types.h>

/* used for calculating console and mouse idle times */
#if defined(Solaris28) || defined(Solaris29) || defined(Solaris10) || defined(Solaris11)
#include <kstat.h>
#endif

#include <unistd.h>
/* These are all functions that unistd.h is supposed to prototype that
   for whatever reason, are not defined on Solaris. */
BEGIN_C_DECLS
long gethostid(void);
int getpagesize(void);
int getdtablesize(void);
int getpriority( int, id_t );
int setpriority( int, id_t, int );
int utimes( const char*, const struct timeval* );
int getdomainname( char*, size_t );
END_C_DECLS

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <sys/select.h>

#define BSD_COMP
#include <sys/ioctl.h>
#undef BSD_COMP

#include <sys/stat.h>
#include <sys/statfs.h>

#if defined(_POSIX_C_SOURCE)
#	define HOLD_POSIX_C_SOURCE _POSIX_C_SOURCE
#	undef _POSIX_C_SOURCE
#endif
#include <sys/wait.h>
#if defined(HOLD_POSIX_C_SOURCE)
#	define _POSIX_C_SOURCE HOLD_POSIX_C_SOURCE 
#	undef HOLD_POSIX_C_SOURCE 
#endif

#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/dirent.h>

BEGIN_C_DECLS
/* This is a system call with no header. */
extern int fdsync( int fd,int flags );
/* I can't find this prototype... */
extern int getdents(int, struct dirent *, unsigned int);
END_C_DECLS

/****************************************
** Condor-specific system definitions
****************************************/

#define HAS_U_TYPES			1
#define SIGSET_CONST			const
#define SYNC_RETURNS_VOID		1

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29) || defined(Solaris10) || defined(Solaris11)
	#define HAS_64BIT_SYSCALLS	1
	#define HAS_64BIT_STRUCTS	1
	#define HAS_F_DUP2FD		1
#endif

typedef void* MMAP_T;

#if defined(Solaris26)
	typedef int socklen_t;
#endif

#endif /* CONDOR_SYS_SOLARIS_H */
