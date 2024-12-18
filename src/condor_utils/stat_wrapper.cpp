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


//
// StatWrapper proper class methods
//
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
		return "condor_fstat";
	} else if ( !m_path.empty() ) {
		if ( m_do_lstat ) {
#if defined(WIN32)
			return "condor_stat";
#else
			return "condor_lstat";
#endif
		} else {
			return "condor_stat";
		}
	} else {
		return NULL;
	}
};


int
StatWrapper::Stat()
{
	if ( m_fd >= 0 ) {
		m_rc = condor_fstat( m_fd, &m_statbuf );
	} else if ( !m_path.empty() ) {
		if ( m_do_lstat ) {
#if defined(WIN32)
			m_rc = condor_stat( m_path.c_str(), &m_statbuf );
#else
			m_rc = condor_lstat( m_path.c_str(), &m_statbuf );
#endif
		} else {
			m_rc = condor_stat( m_path.c_str(), &m_statbuf );
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
