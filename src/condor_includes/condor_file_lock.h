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

#if defined( HPUX9 )
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

#if defined(IRIX53)
#define CONDOR_USE_FLOCK 0
#endif

#if !defined(CONDOR_USE_FLOCK)
ERROR: DONT KNOW WHETHER TO USE FLOCK or FCNTL
#endif

#ifndef SEEK_SET
#	define SEEK_SET 0
#endif

/**********************************************************************
** Grab definition of c++ FileLock class
**********************************************************************/
#include "file_lock.h"


#endif FILE_LOCK_H
