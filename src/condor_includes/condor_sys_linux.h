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
#ifndef CONDOR_SYS_LINUX_H
#define CONDOR_SYS_LINUX_H

#define _BSD_SOURCE

#if defined(GLIBC)
#define _GNU_SOURCE
#endif

#include <sys/types.h>
typedef long rlim_t;
#define HAS_U_TYPES

#define idle _hide_idle
#if defined(GLIBC)
#	define truncate _hide_truncate
#	define ftruncate _hide_ftruncate
#	define profil _hide_profil
#	define daemon _hide_daemon
#endif /* GLIBC */
#include <unistd.h>
#undef idle
#if defined(GLIBC)
#	undef truncate
#	undef ftruncate
#	undef profil
#	undef daemon
BEGIN_C_DECLS
    int truncate( const char *, size_t );
    int ftruncate( int, size_t );
    int profil( char*, int, int, int );
END_C_DECLS
#endif /* GLIBC */

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/* <stdio.h> on glibc Linux defines a "dprintf()" function, which
   we've got hide since we've got our own. */
#if defined(GLIBC)
#	define dprintf _hide_dprintf
#	define getline _hide_getline
#endif
#include <stdio.h>
#if defined(GLIBC)
#	undef dprintf
#	undef getline
#endif

#define SignalHandler _hide_SignalHandler
#include <signal.h>
#undef SignalHandler

/* Since param.h defines NBBY  w/o check, we want to include it here 
   before we check for and define it ourselves */ 
#include <sys/param.h>

/* There is no <sys/select.h> on Linux, select() and friends are 
   defined in <sys/time.h> */
#include <sys/time.h>

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/vfs.h>

#include <sys/wait.h>
/* Some versions of Linux don't define WCOREFLG, but we need it */
#if !defined( WCOREFLG )
#	define WCOREFLG 0200
#endif 

#if defined(GLIBC)
/* glibc defines the 3rd arg of readv and writev to be int, even
   though the man page (and our code) says it should be size_t. 
   -Derek Wright 4/17/98 */
#define readv __hide_readv
#define writev __hide_writev
#endif /* GLIBC */
#include <sys/uio.h>
#if defined(GLIBC)
#undef readv
#undef writev
BEGIN_C_DECLS
int readv(int, const struct iovec *, size_t);
int writev(int, const struct iovec *, size_t);
END_C_DECLS
#endif /* GLIBC */

#if defined(GLIBC) 
/* With the new glibc, there are a bunch of changes in
   prototypes... things that the man pages say are ints that the
   headers say are unsigned, things that are supposed to be const are
   not, etc, etc.  So, we hide the broken prototypes and define our
   own. -Derek 4/17/98 */
#define accept hide_accept
#define getpeername hide_getpeername
#define getsockname hide_getsockname
#define setsockopt hide_setsockopt
#define recvfrom hide_recvfrom
#endif /* GLIBC */
#include <sys/socket.h>
#if defined(GLIBC) 
#undef accept
#undef getpeername
#undef getsockname
#undef setsockopt
#undef recvfrom
BEGIN_C_DECLS
int accept(int, struct sockaddr *, int *);
int getpeername(int, struct sockaddr *, int *);
int getsockname(int, struct sockaddr *, int *);
int setsockopt(int, int, int, const void *, int);
int recvfrom(int, void *, int, unsigned int, struct sockaddr *, int *);
END_C_DECLS
#endif /* GLIBC */

#if defined(GLIBC)
/* glibc defines prototypes for a bunch of functions that are supposed
   to take ints (according to the man page, POSIX, whatever) that
   really take enums.  -Derek Wright 4/17/98 */
#define getpriority __hide_getpriority
#define getrlimit __hide_getrlimit
#define __getrlimit __hide__getrlimit
#define setrlimit __hide_setrlimit
#endif /* GLIBC */
#include <sys/resource.h>
#if defined(GLIBC)
#undef getpriority
#undef getrlimit
#undef __getrlimit
#undef setrlimit
BEGIN_C_DECLS
int getrlimit(int, struct rlimit *);
int __getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
int getpriority(int, int);
END_C_DECLS
#endif /* GLIBC */

#endif /* CONDOR_SYS_LINUX_H */

