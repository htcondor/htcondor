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
#include "condor_debug.h"
#include "write_user_log_state.h"
#include "read_user_log.h"


WriteUserLogState::WriteUserLogState( void )
{
	Clear();
}

WriteUserLogState::~WriteUserLogState( void )
{
	Clear();
}

void
WriteUserLogState::Clear( void )
{
	m_inode = 0;
	m_ctime = 0;
	m_filesize = 0;
}

bool
WriteUserLogState::isNewFile( const struct stat &sinfo ) const
{
	if ( sinfo.st_size < m_filesize ) {
		return true;
	}
# ifdef WIN32
	if ( sinfo.st_ctime != m_ctime ) {
		return true;
	}
# else
	if ( sinfo.st_ino != m_inode ) {
		return true;
	}
# endif
	return false;
}

bool
WriteUserLogState::isOverSize( filesize_t max_size ) const
{
	return ( m_filesize > max_size );
}

bool
WriteUserLogState::Update( const struct stat &sinfo )
{
	m_inode = sinfo.st_ino;
	m_ctime = sinfo.st_ctime;
	m_filesize = sinfo.st_size;

	return true;
}
