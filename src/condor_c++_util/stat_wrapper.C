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
	Clean( true );
	m_path = strdup( path_arg );
	DoStat( m_path );
}

StatWrapper::StatWrapper( int fd_arg )
{
	Clean( true );
	m_fd = fd_arg;
	DoStat( m_fd );
}

StatWrapper::StatWrapper( void )
{
	Clean( true );
}

StatWrapper::~StatWrapper( void )
{
	Clean( false );
}

int
StatWrapper::Stat( const char *path )
{
	Clean( false );
	m_path = strdup( path );
	return DoStat( m_path );
}

int
StatWrapper::Stat( int fd )
{
	Clean( false );
	m_fd = fd;
	return DoStat( m_fd );
}

void
StatWrapper::Clean( bool init )
{	
	if ( init ) {
		m_path = NULL;
	}
	if ( m_path ) {
		free ( m_path );
		m_path = NULL;
	}
	m_fd = -1;
	m_stat_valid = false;
	m_lstat_valid = false;

	m_name = NULL;

	m_stat_rc = 0;
	m_stat_errno = 0;

	memset( &m_stat_buf, 0, sizeof(m_stat_buf) );
	memset( &m_lstat_buf, 0, sizeof(m_lstat_buf) );
}

int
StatWrapper::Retry( void )
{
	if ( m_fd ) {
		return DoStat( m_fd );
	}
	else if ( m_path ) {
		return DoStat( m_path );
	}
	else {
		return -1;
	}
}

bool
StatWrapper::GetStatBuf( StatStructType &buf ) const
{
	if ( ! m_stat_valid ) {
		return false;
	}
	buf = m_stat_buf;
	return true;
}

bool
StatWrapper::GetLstatBuf( StatStructType &buf ) const
{
	if ( ! m_lstat_valid ) {
		return false;
	}
	buf = m_lstat_buf;
	return true;
}

int
StatWrapper::DoStat( const char *path_arg )
{
	int lstat_rc;
	int lstat_errno;

#if HAVE_STAT64
	m_name = "stat64";
#	define STAT_FUNC stat64
#	define LSTAT_FUNC lstat64

#elif HAVE__STATI64
	m_name = "_stati64";
#	define STAT_FUNC _stati64
#	define LSTAT_FUNC _lstati64

#else
	m_name = "stat";
#	define STAT_FUNC stat
#	define LSTAT_FUNC lstat
#endif

	m_stat_rc = ::STAT_FUNC( path_arg, &m_stat_buf );
	m_stat_errno = errno;
#if !defined(WIN32)
	lstat_rc = ::LSTAT_FUNC( path_arg, &m_lstat_buf );
	lstat_errno = errno;
#else
	// Windows doesn't have lstat, so just copy the stat_buf
	// into lstat_buf
	lstat_rc = 0;
	lstat_errno = 0;
	m_lstat_buf = m_stat_buf;
#endif

	// Set the valid flags
	if ( 0 == m_stat_rc ) {
		m_stat_valid = true;
	}
	if ( 0 == lstat_rc ) {
		m_lstat_valid = true;
	}

	if ( m_lstat_valid && !m_stat_valid ) {
		m_stat_rc = lstat_rc;
		m_stat_errno = lstat_errno;
	}
	return m_stat_rc;
}

int
StatWrapper::DoStat( int fd_arg )
{
#if HAVE_STAT64
	m_name = "fstat64";
	m_stat_rc = ::fstat64( fd_arg, &m_stat_buf );
#elif HAVE__STATI64
	m_name = "_fstati64";
	m_stat_rc = ::_fstati64( fd_arg, &m_stat_buf );
#else
	m_name = "fstat";
	m_stat_rc = ::fstat( fd_arg, &m_stat_buf );
#endif

	// Set the valid flags
	if ( 0 == m_stat_rc ) {
		m_stat_valid = true;
		m_lstat_valid = true;
	}

	m_stat_errno = errno;
	m_lstat_buf = m_stat_buf;
	return m_stat_rc;
}
