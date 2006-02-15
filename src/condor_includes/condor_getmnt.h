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
#ifndef _CONDOR_GETMNT_H
#define _CONDOR_GETMNT_H

#if defined(ULTRIX42) || defined(ULTRIX43)
#	include <sys/mount.h>
#endif

#if !defined(OSF1) && !defined(ULTRIX42) && !defined(ULTRIX43) && !defined(AIX32) && !defined(Solaris) && !defined(WIN32) && !defined(Darwin)
#	include <mntent.h>
#endif

/* Solaris specific change ..dhaval 6/26 */
#if defined(Solaris)
#include <sys/mnttab.h>
#endif

#if defined(AIX32)
#	include <sys/mntctl.h>
#	include <sys/vmount.h>
#	include <sys/sysmacros.h>
#endif

#if !defined(NMOUNT)
#define NMOUNT 256
#endif

#if !defined(ULTRIX42) && !defined(ULTRIX43)
struct fs_data_req {
	dev_t	dev;
	char	*devname;
	char	*path;
};
struct fs_data {
	struct fs_data_req fd_req;
};
#define NOSTAT_MANY 0
#endif

#if NMOUNT < 256
#undef  NMOUNT
#define NMOUNT  256
#endif

#endif /* _CONDOR_GETMNT_H */
