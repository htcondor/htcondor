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
#ifndef CONDOR_SYSTEM_H
#define CONDOR_SYSTEM_H

#if defined(WIN32)

/******************************
** Windows specifics
******************************/
#include "condor_sys_nt.h"

#else

/******************************
** Unix specifics
******************************/

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

#endif /* CONDOR_SYSTEM_H */




