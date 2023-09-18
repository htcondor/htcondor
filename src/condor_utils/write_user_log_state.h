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
#ifndef _CONDOR_WRITE_USER_LOG_STATE_CPP_H
#define _CONDOR_WRITE_USER_LOG_STATE_CPP_H

#include "condor_common.h"
#include "stat_wrapper.h"

class WriteUserLogState
{
public:
    ///
    WriteUserLogState( void );
	~WriteUserLogState( void );

	// Accessors
	void getFileSize( filesize_t &filesize ) const { filesize = m_filesize; };

	// Comparison operators
	bool isNewFile(  const StatWrapper &stat ) const;
	bool isOverSize( filesize_t max_size ) const;

	// Update the state
	bool Update( const StatWrapper &stat );
	void Clear( void );

private:

	StatStructInode	m_inode;
	time_t			m_ctime;
	filesize_t		m_filesize;
};

#endif /* _CONDOR_USER_LOG_CPP_STATE_H */
