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
#ifndef CONDOR_SYS_SOLARIS_H
#define CONDOR_SYS_SOLARIS_H

#define __EXTENSIONS__

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29)
#	define _LARGEFILE64_SOURCE
#endif

#include "condor_fix_sys_utsname.h"
#include "condor_fix_sys_stat.h"

#include <sys/types.h>

/* used for calculating console and mouse idle times */
#if defined(Solaris28) || defined(Solaris29)
#include <kstat.h>
#endif

#include <unistd.h>
/* These are all functions that unistd.h is supposed to prototype that
   for whatever reason, are not defined on Solaris. */
BEGIN_C_DECLS
int gethostname( char *, int );
long gethostid();
int getpagesize();
int getdtablesize();
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

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29)
	#define HAS_64BIT_SYSCALLS	1
	#define HAS_64BIT_STRUCTS	1
	#define HAS_F_DUP2FD		1
#elif defined(Solaris251)
	typedef long long off64_t;
#endif

typedef void* MMAP_T;

#if defined(Solaris251) || defined(Solaris26)
	typedef int socklen_t;
#endif

#endif /* CONDOR_SYS_SOLARIS_H */
