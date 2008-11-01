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


#include "condor_common.h"
#include "condor_config.h"
#include "stat_wrapper.h"


StatWrapperIntBase::StatWrapperIntBase( const StatWrapperIntBase &other )
{
	other.GetBuf( m_buf );
	m_name = other.GetFnName( );
	m_valid = other.IsValid( );
	m_rc = other.GetRc( );
	m_errno = other.GetErrno( );
}

StatWrapperIntBase::StatWrapperIntBase( const char *name )
{
	m_name = name;
	m_valid = false;
	m_buf_valid = false;
	m_rc = 0;
	m_errno = 0;
}

int
StatWrapperIntBase::CheckResult( void )
{
	if ( 0 == m_rc ) {
		m_buf_valid = true;
		m_errno = 0;
	}
	else {
		m_errno = errno;
		m_buf_valid = false;
	}
	return m_rc;
}

//
// StatWrapper internal -- Path Version
//

// Basic constructor
StatWrapperIntPath::StatWrapperIntPath(
	const char *name,
	int (* const fn)(const char *, StatStructType *) )
		: StatWrapperIntBase( name ), m_fn( fn ), m_path( NULL )
{
	// Do nothing
}

// Copy constructor
StatWrapperIntPath::StatWrapperIntPath( const StatWrapperIntPath &other )
		: StatWrapperIntBase( other ), m_fn( other.GetFn() )
{
	SetPath( other.GetPath() );
}

// Destructor
StatWrapperIntPath::~StatWrapperIntPath( void )
{
	if ( m_path ) {
		free( const_cast<char*>(m_path) );
		m_path = NULL;
	}
}

bool
StatWrapperIntPath::SetPath( const char *path )
{
	if ( m_path && strcmp( path, m_path ) ) {
		free( const_cast<char*>(m_path) );
		m_path = NULL;
	}
	if ( path ) {
		if ( !m_path ) {
			m_path = strdup( path );
		}
		m_valid = true;
	} else {
		m_valid = false;
	}
	m_buf_valid = false;
	m_rc = 0;
	return true;
}

int
StatWrapperIntPath::Stat( bool force )
{
	if ( !m_fn ) {
		return SetRc( -2 );
	}
	if ( !m_path ) {
		return SetRc( -3 );
	}
	if ( m_valid && !force ) {
		return GetRc( );
	}
	m_rc = m_fn( m_path, &m_buf );
	return CheckResult( );
}

//
// StatWrapper internal -- FD Version
//

// FD Version: do the stat
StatWrapperIntFd::StatWrapperIntFd(
	const char *name,
	int (* const fn)(int, StatStructType *) )
		: StatWrapperIntBase( name ), m_fn( fn ), m_fd( -1 )
{
	// Do nothing
}

StatWrapperIntFd::StatWrapperIntFd( const StatWrapperIntFd &other )
		: StatWrapperIntBase( other ),
		  m_fn( other.GetFn() ),
		  m_fd( other.GetFd() )
{
	// Do nothing
}

StatWrapperIntFd::~StatWrapperIntFd( void )
{
	// Do nothing
}

// Set the FD
bool
StatWrapperIntFd::SetFD( int fd )
{
	if ( fd != m_fd ) {
		m_buf_valid = false;
		m_rc = 0;
	}
	if ( fd >= 0 ) {
		m_valid = true;
	} else {
		m_valid = false;
	}
	m_fd = fd;
	return true;
}

int
StatWrapperIntFd::Stat( bool force )
{
	if ( !m_fn ) {
		return SetRc( -2 );
	}
	if ( m_fd < 0 ) {
		return SetRc( -3 );
	}
	if ( m_valid && !force ) {
		return GetRc( );
	}
	m_rc = m_fn( m_fd, &m_buf );
	return CheckResult( );
}


//
// StatWrapper internal -- NOP Version
//

// FD Version: do the stat
StatWrapperIntNop::StatWrapperIntNop(
	const char *name,
	int (* const fn)(int, StatStructType *) )
		: StatWrapperIntBase( name ), m_fn( fn )
{
	// Do nothing
}

StatWrapperIntNop::StatWrapperIntNop( const StatWrapperIntNop &other )
		: StatWrapperIntBase( other ),
		  m_fn( other.GetFn() )
{
	// Do nothing
}

StatWrapperIntNop::~StatWrapperIntNop( void )
{
	// Do nothing
}

int
StatWrapperIntNop::Stat( bool force )
{
	(void) force;
	return 0;
}


