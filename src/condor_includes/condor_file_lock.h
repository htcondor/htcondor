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

#if defined(Darwin)
#define CONDOR_USE_FLOCK 1 
#include <sys/file.h>
#endif

#if defined(CONDOR_FREEBSD)
#define CONDOR_USE_FLOCK 1 
#include <sys/file.h>
#endif

#if defined(WIN32)
#define CONDOR_USE_FLOCK 0		// does not matter on Win32 since we use lock_file.WIN32.c
#include "fake_flock.h"
#endif

#if defined( AIX )
#define CONDOR_USE_FLOCK 1
#include <sys/file.h>
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
