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

#ifndef STAT_ACCESS_H
#define STAT_ACCESS_H

#include "stat_struct.h"

	// Define a class to provide access to the members of the stat
	// structure.
class StatAccess
{
  public:
	StatAccess( void ) {
		clear( );
	};
	StatAccess( const StatStructType &statbuf ) {
		update( statbuf );
	};
	~StatAccess( void ) { };

	// Update / clear the stat buffer
	void clear( void ) {
		m_valid = false;
		memset( &m_statbuf, 0, sizeof(StatStructType) );
	};
	void update( const StatStructType &statbuf ) {
		m_valid = true;
		memcpy( &m_statbuf, &statbuf, sizeof(StatStructType) );
	};

	// Does the stat buffer look valid?
	bool isValid( void ) const {
		return m_valid;
	};

	// Accessors
	StatStructDev getDev( void ) const {
		return m_statbuf.st_dev;
	};
	StatStructInode getInode( void ) const {
		return m_statbuf.st_ino;
	};
	StatStructMode getMode( void ) const {
		return m_statbuf.st_mode;
	};
	StatStructNlink getNlink( void ) const {
		return m_statbuf.st_nlink;
	};
	StatStructUID getUid( void ) const {
		return m_statbuf.st_uid;
	};
	StatStructGID getGid( void ) const {
		return m_statbuf.st_gid;
	};
	StatStructDev getRdev( void ) const {
		return m_statbuf.st_rdev;
	};
	StatStructOff getSize( void ) const {
		return m_statbuf.st_size;
	};

# if STAT_STRUCT_HAVE_BLOCK_SIZE
	StatStructBlockSize getBlockSize( void ) const {
		return m_statbuf.st_blksize;
	};
# endif	

# if STAT_STRUCT_HAVE_BLOCK_COUNT
	StatStructBlockCount getBlockCount( void ) const {
		return m_statbuf.st_blocks;
	};
# endif

	StatStructTime getAtime( void ) const {
		return m_statbuf.st_atime;
	};
	StatStructTime getMtime( void ) const {
		return m_statbuf.st_mtime;
	};
	StatStructTime getCtime( void ) const {
		return m_statbuf.st_ctime;
	};

	const StatStructType & getStatBuf( void ) const {
		return m_statbuf;
	};
	StatStructType & getStatBufRw( void ) {
		return m_statbuf;
		
	};

  private:
	bool			m_valid;
	StatStructType	m_statbuf;
	
};

#endif
