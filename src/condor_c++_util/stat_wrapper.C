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

#include "condor_common.h"
#include "condor_config.h"
#include "stat_wrapper.h"

// Ugly hack: stat64 prototyps are wacked on HPUX, so define them myself
// These are cut & pasted from the HPUX man pages...
#if defined( HPUX )
extern "C" {
	extern int stat64(const char *path, struct stat64 *buf);
	extern int fstat64(int fildes, struct stat64 *buf);
}
#endif

StatWrapper::StatWrapper( const char *path )
{
	this->path = strdup( path );
	this->fd = -1;
	DoStat( this->path );
}

StatWrapper::StatWrapper( int fd )
{
	this->path = NULL;
	this->fd = fd;
	DoStat( fd );
}

StatWrapper::~StatWrapper( void )
{
	if ( path ) {
		free ( (void*) path );
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
StatWrapper::DoStat( const char *path )
{
#if defined HAS_STAT64
	name = "stat64";
	status = ::stat64( path, &buf );
#elif defined HAS__STATI64
	name = "_stati64";
	status = ::_stati64( path, &buf );
#else
	name = "stat";	
	status = ::stat( path, &buf );
#endif

	stat_errno = errno;
	return status;
}

int
StatWrapper::DoStat( int fd )
{
#if defined HAS_STAT64
	name = "fstat64";
	status = ::fstat64( fd, &buf );
#elif defined HAS__STATI64
	name = "_fstati64";
	status = ::_fstati64( fd, &buf );
#else
	name = "fstat";
	status = ::fstat( fd, &buf );
#endif

	stat_errno = errno;
	return status;
}
