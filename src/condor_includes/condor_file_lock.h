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
