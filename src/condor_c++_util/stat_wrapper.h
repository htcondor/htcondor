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

#ifndef STAT_WRAPPER_H
#define STAT_WRAPPER_H

#include "condor_common.h"
#include "condor_config.h"

	// Define a "standard" StatBuf type
#if HAVE_STAT64
typedef struct stat64 StatStructType;
typedef ino_t StatStructInode;
#elif HAVE__STATI64
typedef struct _stati64 StatStructType;
typedef _ino_t StatStructInode;
#else
typedef struct stat StatStructType;
typedef ino_t StatStructInode;
#endif

class StatWrapper
{
public:
	StatWrapper( const char *path );
	StatWrapper( int fd );
	StatWrapper( void );
	~StatWrapper( );
	int Retry( void );

	int Stat( const char *path );
	int Stat( int fd );

	const char *GetStatFn( void ) const { return m_name; };
	int GetStatus( void ) const { return m_stat_rc; };
	int GetErrno( void ) const { return m_stat_errno; };
	const StatStructType *GetStatBuf( void ) const { return &m_stat_buf; };
	const StatStructType *GetLstatBuf( void ) const { return &m_lstat_buf; };
	bool GetStatBuf( StatStructType & ) const;
	bool GetLstatBuf( StatStructType & ) const;

private:
	void Clean( bool init );
	int DoStat( const char *path );
	int DoStat( int fd );

	StatStructType	m_stat_buf;
	StatStructType	m_lstat_buf;
	bool			m_stat_valid;
	bool			m_lstat_valid;
	const char		*m_name;
	int				m_stat_rc;		// stat() return code
	int				m_stat_errno;	// errno from stat()
	int				m_fd;
	char			*m_path;
};


#endif
