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

#include "condor_common.h"
#include "condor_config.h"
#include "stat_wrapper.h"

// Ugly hack: stat64 prototyps are wacked on HPUX, so define them myself
// These are cut & pasted from the HPUX man pages...
#if defined( HPUX )
extern "C" {
	extern int stat64(const char *path, struct stat64 *buf);
	extern int lstat64(const char *path, struct stat64 *buf);
	extern int fstat64(int fildes, struct stat64 *buf);
}
#endif

StatWrapper::StatWrapper( const char *path_arg )
{
	this->path = strdup( path_arg );
	this->fd = -1;
	DoStat( this->path );
}

StatWrapper::StatWrapper( int fd_arg )
{
	this->path = NULL;
	this->fd = fd_arg;
	DoStat( fd );
}

StatWrapper::~StatWrapper( void )
{
	if ( path ) {
		free ( path );
		path = NULL;
	}
}

int
StatWrapper::Retry( void )
{
	if ( fd ) {
		return DoStat( fd );
	} else {
		return DoStat( path );
	}
}

int
StatWrapper::DoStat( const char *path_arg )
{
	int lstat_rc;
	int lstat_errno;

#if HAVE_STAT64
	name = "stat64";
#	define STAT_FUNC stat64
#	define LSTAT_FUNC lstat64

#elif HAVE__STATI64
	name = "_stati64";
#	define STAT_FUNC _stati64
#	define LSTAT_FUNC _lstati64

#else
	name = "stat";
#	define STAT_FUNC stat
#	define LSTAT_FUNC lstat
#endif

	status = ::STAT_FUNC( path_arg, &stat_buf );
	stat_errno = errno;
#if !defined(WIN32)
	lstat_rc = ::LSTAT_FUNC( path_arg, &lstat_buf );
	lstat_errno = errno;
#else
	// Windows doesn't have lstat, so just copy the stat_buf
	// into lstat_buf
	lstat_rc = 0;
	lstat_errno = 0;
	lstat_buf = stat_buf;
#endif

	if ( status == 0 && lstat_rc != 0 ) {
		status = lstat_rc;
		stat_errno = lstat_errno;
	}
	return status;
}

int
StatWrapper::DoStat( int fd_arg )
{
#if HAVE_STAT64
	name = "fstat64";
	status = ::fstat64( fd_arg, &stat_buf );
#elif HAVE__STATI64
	name = "_fstati64";
	status = ::_fstati64( fd_arg, &stat_buf );
#else
	name = "fstat";
	status = ::fstat( fd_arg, &stat_buf );
#endif

	stat_errno = errno;
	lstat_buf = stat_buf;
	return status;
}
