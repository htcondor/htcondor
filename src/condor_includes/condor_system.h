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
#ifndef CONDOR_SYSTEM_H
#define CONDOR_SYSTEM_H


/******************************
** Windows specifics
******************************/
#if defined(WIN32)
# include "condor_sys_nt.h"
#endif

/********************************************************************
** Declare base system types (currently the int64_t types, but will
** probably grow beyond this charter).
*********************************************************************/
#include "condor_sys_types.h"


/******************************
** Unix specifics
******************************/
#if !defined( WIN32 )

/* Things we want defined on all Unix systems */

#ifndef _POSIX_SOURCE
# define _POSIX_SOURCE
#endif

#ifndef UNIX
# define UNIX
#endif


/**********************************************************************
** These system-specific files will "fix" anything that needs fixing,
** define platform-specific functionality flags we want to use, etc. 
**********************************************************************/	

#if defined(LINUX)
#	include "condor_sys_linux.h"
#elif defined(IRIX)
#	include "condor_sys_irix.h"
#elif defined(HPUX)
#	include "condor_sys_hpux.h"
#elif defined(Solaris)
#	include "condor_sys_solaris.h"
#elif defined(OSF1)
#	include "condor_sys_dux.h"
#elif defined(Darwin)
#	include "condor_sys_bsd.h"
#elif defined(AIX)
#	include "condor_sys_aix.h"
#else
#   error "condor_system.h: Don't know what Unix this is!"
#endif


/**********************************************************************
** Files that need to be fixed on all (or nearly all) platforms 
**********************************************************************/
#include "condor_fix_access.h"
#include "condor_file_lock.h"
#include "condor_fix_assert.h"


/**********************************************************************
** Clean-up, default definitions, etc.
**********************************************************************/

#if !defined(HAS_U_TYPES)
    typedef unsigned int	u_int;
    typedef unsigned char	u_char;
    typedef unsigned short	u_short;
    typedef unsigned long	u_long;
#endif

#if !defined(NO_VOID_SIGNAL_RETURN)
#	define VOID_SIGNAL_RETURN	1
#endif

#if !defined(SIGISMEMBER_IS_BROKEN)
#	define SIGISMEMBER_IS_BROKEN 0
#endif

#if !defined(HAS_F_DUP2FD)
#	define HAS_F_DUP2FD 0
#endif

#if !defined(NBBY)
#	define NBBY 8
#endif

#if !defined(NFDS)
#	define NFDS(x) (x)
#endif


/* If WCOREFLAG is defined but WCOREFLG is not, define WCOREFLG since
   that's what we use in our code. */
#if defined(WCOREFLAG) && !defined(WCOREFLG)
#	define WCOREFLG WCOREFLAG
#endif

#ifndef WORD_BIT
#	define WORD_BIT 32
#endif 

#ifndef _POSIX_PATH_MAX
#	define _POSIX_PATH_MAX 255
#endif

#ifndef _POSIX_ARG_MAX
#	define _POSIX_ARG_MAX 4096
#endif


/**********************************************************************
** We'll also include everything we use after this so we make sure
** we've got what we need on all platforms.  Headers that have already
** been fixed will simply be ignored since they define their own flags
** to prevent multiple inclusions. 
**********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/utsname.h>		
#include <sys/resource.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>	
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <rpc/types.h>
#if !defined(Darwin)
#include <values.h>
#endif
#include <math.h>
#include <utime.h>
#if !defined(Darwin)
#include <sys/poll.h>
#endif

#define stricmp strcasecmp		/* stricmp no longer exits in egcs, but strcasecmp does */
#define strincmp strncasecmp	/* strincmp no longer exits in egcs, but strncasecmp does */

#include "condor_snutils.h"

/* select() on all our platforms takes an fd_set pointer, so we can
   just define this here for everyone.  We don't really need it
   anymore, but we might hit a platform that has a different select,
   so the level of indirection is worth keeping around. */
typedef fd_set *SELECT_FDSET_PTR;

#endif /* UNIX */


/* This stuff here is shared by all archs, *INCLUDING* NiceTry */

/* Pull in some required types */
#if defined( HAVE_SYS_TYPES_H )
# include <sys/types.h>
#endif
#if defined( HAVE_STDINT_H )
# include <stdint.h>
#endif
#if defined( HAVE_INTTYPES_H )
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
#endif

/* Define _O_BINARY, etc. if they're not defined as zero */
#ifndef _O_BINARY
 #define _O_BINARY		0x0
#endif
#ifndef _O_SEQUENTIAL
 #define _O_SEQUENTIAL	0x0
#endif
#ifndef O_LARGEFILE
 #define O_LARGEFILE	0x0
#endif

/* If no inttypes, try to define our own (make a guess) */
/* The win 32 include file defines the win32 versions */
#if !defined( PRId64 )
# define PRId64 "lld"
#endif
#if !defined( PRIi64 )
# define PRIi64 "lli"
#endif
#if !defined( PRIu64 )
# define PRIu64 "llu"
#endif

// Define a 'filesize_t' type and FILESIZE_T_FORMAT printf format string
#if defined HAVE_INT64_T
  typedef int64_t filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#elif defined HAVE___INT64_T
  typedef __int64 filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#else
  typedef long filesize_t;
# define FILESIZE_T_FORMAT "%l"
#endif

#endif /* CONDOR_SYSTEM_H */
