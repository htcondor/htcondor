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
#ifndef CONDOR_FILE_LOCK_H
#define CONDOR_FILE_LOCK_H

/**********************************************************************
** Include whatever headers each platform needs to define LOCK_UN,
** LOCK_SH, etc, and define CONDOR_USE_FLOCK appropriately to
** determine how to implement file locking on this platform.
** For most platforms, we just need to include <sys/file.h>.  On others,
** this file either doesn't exist, or doesn't define what we need, so
** we include "fake_flock.h" instead, which defines them for us.
**********************************************************************/

#if defined( HPUX ) 
#define CONDOR_USE_FLOCK 0
#include "fake_flock.h" 
#endif

#if defined( AIX32 )
#define CONDOR_USE_FLOCK 0
#endif

#if defined( SUNOS41 )
#define CONDOR_USE_FLOCK 1
#include <sys/file.h>
#endif

#if defined( OSF1 )
#define CONDOR_USE_FLOCK 1
#include <sys/file.h>
#endif

#if defined( LINUX)
#define CONDOR_USE_FLOCK 1
#include <sys/file.h>
#endif

#if defined(Solaris)
#define CONDOR_USE_FLOCK 0
#include "fake_flock.h"
#endif

#if defined(IRIX)
#define CONDOR_USE_FLOCK 0
#include <sys/file.h>
#endif

#if defined(WIN32)
#define CONDOR_USE_FLOCK 0		// does not matter on Win32 since we use lock_file.WIN32.c
#include "fake_flock.h"
#endif

#if !defined(CONDOR_USE_FLOCK)
#error ERROR: DONT KNOW WHETHER TO USE FLOCK or FCNTL
#endif

#ifndef SEEK_SET
#	define SEEK_SET 0
#endif

/**********************************************************************
** Grab definition of c++ FileLock class
**********************************************************************/
#include "file_lock.h"


#endif /* FILE_LOCK_H */
