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
#ifndef _CONDOR_GETMNT_H
#define _CONDOR_GETMNT_H

#if defined(ULTRIX42) || defined(ULTRIX43)
#	include <sys/mount.h>
#endif

#if !defined(OSF1) && !defined(ULTRIX42) && !defined(ULTRIX43) && !defined(AIX32) && !defined(Solaris) && !defined(WIN32)
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
