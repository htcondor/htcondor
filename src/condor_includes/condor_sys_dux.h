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
#ifndef CONDOR_SYS_DUX_H
#define CONDOR_SYS_DUX_H

/* We need to define _OSF_SOURCE so that type quad, and u_int and
   friends get defined.  We should leave it on since we'll need it
   later as well. */
#ifndef _OSF_SOUCE
# define _OSF_SOURCE
#endif
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE
#endif
#ifndef _XOPEN_SOURCE_EXTENDED
# define _XOPEN_SOURCE_EXTENDED
#endif
#ifndef _AES_SOURCE
# define _AES_SOURCE
#endif

#include <sys/types.h>
/* The system include file defines this in terms of bzero(), but ANSI
   says we should use memset(). */
#undef FD_ZERO
#define FD_ZERO(p)     memset((char *)(p), 0, sizeof(*(p)))

/* bzero has different types in these two files.  Let's choose one. */

#define bzero __condor_hack_bzero
#include <strings.h>
#undef bzero
#include <sys/select.h>

/* Include sysmisc.h for the proc library now, but it needs to _not_
 * have _XOPEN_SOURCE_EXTENDED defined, cuz we need struct sigaltstack. 
 * And we must include it
 * _after_ we have included sys/types.h.  Sigh.  -Todd 8/19/98 */
#undef _XOPEN_SOURCE_EXTENDED
#include <sys/sysmisc.h>
#define _XOPEN_SOURCE_EXTENDED

#include <unistd.h>
#if defined(pipe)
#	undef pipe
BEGIN_C_DECLS
int pipe( int filedes[2] );
END_C_DECLS
#endif /* defined(pipe) */

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>
#include <stdio.h>
#include <sys/select.h>

#if !defined(_LIBC_POLLUTION_H_)
#define _LIBC_POLLUTION_H_
#define CONDOR_LIBC_POLLUTION_H_
#endif
#include <signal.h>
#if defined(CONDOR_LIBC_POLLUTION_H_)
#undef _LIBC_POLLUTION_H_
#undef CONDOR_LIBC_POLLUTION_H_
#endif

#if !defined(NSIG) && defined(SIGMAX)
#define NSIG (SIGMAX+1)
#endif

/* Provide a prototype for _Esigaction, otherwise C++ thinks it is a constructor for struct sigaction */

int _Esigaction(int signum, const struct sigaction  *act, struct sigaction *oldact);

/* And, redirect calls to sigaction to _Esigaction like the header file wants */
#define sigaction(a,b,c) _Esigaction(a,b,c)

/* Need these to get statfs and friends defined */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <sys/wait.h>
#include <sys/resource.h>

#if !defined(_LIBC_POLLUTION_H_)
#define _LIBC_POLLUTION_H_
#define CONDOR_LIBC_POLLUTION_H_
#endif
#include <sys/uio.h>
#if defined(CONDOR_LIBC_POLLUTION_H_)
#undef _LIBC_POLLUTION_H_
#undef CONDOR_LIBC_POLLUTION_H_
#endif

/* If _XOPEN_SOURCE_EXTENDED is defined on dux4.0 when you include
   sys/socket.h "accept" gets #define'd, which screws up condor_io,
   since we have methods called "accept" in there.  -Derek 3/26/98 */
#undef _XOPEN_SOURCE_EXTENDED
#include <sys/socket.h>
#define _XOPEN_SOURCE_EXTENDED

/* We need to include this _before_ 'db.h' gets included so that we
   get the int64_t types */
#if HAVE_INTTYPES_H
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
#endif

/* On DUX4.0, netdb.h defines gethostid to return an int, while
   unistd.h says it returns a long.  Just hide the definition here. */
#define gethostid _hide_gethostid
#include <netdb.h>
#undef gethostid

/* On Digital Unix, basename() has the wrong prototype.  Normally,
   we'd "hide" it, but the system header files are already doing wierd
   things with it, like "#define basename _Ebasename", etc.  So, to
   effectively hide it, all we have to do is undef what the system
   header files have done. -Derek Wright 10/30/98 */
#include <string.h>
#undef basename
#undef dirname

/* These don't seem to have prototypes in OSF */

BEGIN_C_DECLS
int async_daemon(void);
int nfssvc(int,int,int);
int swapon(const char *,int);
int initgroups(const char *, gid_t);	/* DEC screwed up the signature on this one */
int sigblock( int mask );
int sigsetmask( int mask );
END_C_DECLS

/* Needed for MAXINT */
#include <values.h>

/* Needed for PIOCPSINFO */
#include <sys/procfs.h>

/****************************************
** Condor-specific system definitions
****************************************/

#define HAS_U_TYPES		1
#define SIGSET_CONST		const
#define SYNC_RETURNS_VOID	1
#define NO_VOID_SIGNAL_RETURN	1

typedef long long off64_t;
typedef caddr_t MMAP_T;

#endif /* CONDOR_SYS_DUX_H */
