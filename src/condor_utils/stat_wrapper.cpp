/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_config.h"
#include "stat_wrapper.h"

// These are the actual stat functions that we'll use below
#if defined(HAVE_STAT64) && !defined(DARWIN)
#  define STAT_FUNC stat64
   const char *STAT_NAME = "stat64";
#elif defined(HAVE__STATI64)
#  define STAT_FUNC _stati64
   const char *STAT_NAME = "_stati64";
#else
#  define STAT_FUNC stat
   const char *STAT_NAME = "stat";
#endif

// Ditto for lstat.  NOTE: We fall back to stat if there is no lstat
#if defined(HAVE_LSTAT64) && !defined(DARWIN)
#  define LSTAT_FUNC lstat64
   const char *LSTAT_NAME = "lstat64";
#  define HAVE_AN_LSTAT
#elif defined(HAVE__LSTATI64)
#  define LSTAT_FUNC _lstati64
   const char *LSTAT_NAME = "_lstati64";
#  define HAVE_AN_LSTAT
#elif defined(HAVE_LSTAT)
#  define LSTAT_FUNC lstat
   const char *LSTAT_NAME = "lstat";
#  define HAVE_AN_LSTAT
#else
#  define LSTAT_FUNC STAT_FUNC
   const char *LSTAT_NAME = STAT_NAME;
#  undef HAVE_AN_LSTAT
#endif

// These are the actual fstat functions that we'll use below
#if defined(HAVE_FSTAT64) && !defined(DARWIN)
#  define FSTAT_FUNC fstat64
   const char *FSTAT_NAME = "fstat64";
#elif defined(HAVE__FSTATI64)
#  define FSTAT_FUNC _fstati64
   const char *FSTAT_NAME = "_fstati64";
#else
#  define FSTAT_FUNC fstat
   const char *FSTAT_NAME = "fstat";
#endif


//
// StatWrapper proper class methods
//
StatWrapper::StatWrapper( const MyString &path, bool do_lstat )
	: m_rc( 0 ),
	  m_errno( 0 ),
	  m_fd( -1 ),
	  m_do_lstat( do_lstat ),
	  m_buf_valid( false )
{
	memset( &m_statbuf, 0, sizeof(StatStructType) );

	if ( !path.empty() ) {
		m_path = path.c_str();
		Stat();
	}
}

StatWrapper::StatWrapper( const std::string &path, bool do_lstat )
	: m_rc( 0 ),
	  m_errno( 0 ),
	  m_fd( -1 ),
	  m_do_lstat( do_lstat ),
	  m_buf_valid( false )
{
	memset( &m_statbuf, 0, sizeof(StatStructType) );

	if ( !path.empty() ) {
		m_path = path;
		Stat();
	}
}

StatWrapper::StatWrapper( const char *path, bool do_lstat )
	: m_rc( 0 ),
	  m_errno( 0 ),
	  m_fd( -1 ),
	  m_do_lstat( do_lstat ),
	  m_buf_valid( false )
{
	memset( &m_statbuf, 0, sizeof(StatStructType) );

	if ( path != NULL ) {
		m_path = path;
		Stat();
	}
}

StatWrapper::StatWrapper( int fd )
	: m_rc( 0 ),
	  m_errno( 0 ),
	  m_fd( fd ),
	  m_do_lstat( false ),
	  m_buf_valid( false )
{
	memset( &m_statbuf, 0, sizeof(StatStructType) );

	if ( m_fd > 0 ) {
		Stat();
	}
}

StatWrapper::StatWrapper( void )
	: m_rc( 0 ),
	  m_errno( 0 ),
	  m_fd( -1 ),
	  m_do_lstat( false ),
	  m_buf_valid( false )
{
	memset( &m_statbuf, 0, sizeof(StatStructType) );
}

StatWrapper::~StatWrapper( void )
{
}

bool
StatWrapper::IsInitialized( void ) const
{
	return !m_path.empty() || m_fd >= 0;
}

// Set the path(s)
void
StatWrapper::SetPath( const MyString &path, bool do_lstat )
{
	SetPath( path.c_str(), do_lstat );
}

// Set the path(s)
void
StatWrapper::SetPath( const char *path, bool do_lstat )
{
	m_buf_valid = false;
	m_fd = -1;
	if ( path ) {
		m_path = path;
	} else {
		m_path.clear();
	}
	m_do_lstat = do_lstat;
}

void
StatWrapper::SetFD( int fd )
{
	m_buf_valid = false;
	m_path.clear();
	m_fd = fd;
}

const char *
StatWrapper::GetStatFn() const
{
	if ( m_fd >= 0 ) {
		return FSTAT_NAME;
	} else if ( !m_path.empty() ) {
		if ( m_do_lstat ) {
			return LSTAT_NAME;
		} else {
			return STAT_NAME;
		}
	} else {
		return NULL;
	}
};


int
StatWrapper::Stat()
{
	if ( m_fd >= 0 ) {
		m_rc = FSTAT_FUNC( m_fd, &m_statbuf );
	} else if ( !m_path.empty() ) {
		if ( m_do_lstat ) {
			m_rc = LSTAT_FUNC( m_path.c_str(), &m_statbuf );
		} else {
			m_rc = STAT_FUNC( m_path.c_str(), &m_statbuf );
		}
	} else {
		return -3;
	}

	if ( m_rc == 0 ) {
		m_buf_valid = true;
		m_errno = 0;
	} else {
		m_buf_valid = false;
		m_errno = errno;
	}

	return m_rc;
}

// Path specific Stat() - Will run stat() or lstat()
int
StatWrapper::Stat( const MyString &path, bool do_lstat )
{
	SetPath( path, do_lstat );

	return Stat();
}

// Path specific Stat() - Will run stat() or lstat()
int
StatWrapper::Stat( const char *path, bool do_lstat )
{
	SetPath( path, do_lstat );

	return Stat();
}

// FD specific Stat()
int
StatWrapper::Stat( int fd )
{
	SetFD( fd );

	return Stat();
}
