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

#include "MyString.h"
#include "stat_struct.h"
#include "stat_wrapper_internal.h"

// Pre-declare internal class
class StatWrapperIntBase;
class StatWrapperIntPath;
class StatWrapperIntFd;
class StatWrapperIntNop;
class StatWrapperOp;


class StatWrapper
{
public:
	// Operation selector
	// Valid ops for the constructor and Stat() methods:
	//  _NONE, _STAT, _LSTAT, _BOTH, _FSTAT, _ALL, _LAST
	// Valid ops for for the "get" methods:
	//  _LSTAT, _STAT, _FSTAT, and _LAST
	enum StatOpType {
		STATOP_NONE = 0,			// No operation
		STATOP_MIN = STATOP_NONE,	// Min operation #
		STATOP_STAT,				// stat()
		STATOP_LSTAT,				// last() (when available)
		STATOP_BOTH,				// Both stat() and lstat()
		STATOP_FSTAT,				// fstat()
		STATOP_ALL,					// All of stat(), lstat() and fstat()
		STATOP_LAST,				// Last operation
		STATOP_MAX = STATOP_LAST,	// Max operation #
		STATOP_NUM,					// # of operations
	};

	// Constructors & destructors
	StatWrapper( const char *path, StatOpType which = STATOP_STAT );
	StatWrapper( const MyString &path, StatOpType which = STATOP_STAT );
	StatWrapper( int fd, StatOpType which = STATOP_FSTAT );
	StatWrapper( void );
	~StatWrapper( void );

	// Check to see if we're initialized
	bool IsInitialized( void ) const;

	// Methods to set the path and FD
	bool SetPath( const char *path );
	bool SetPath( const MyString &path );
	bool SetFD( int fd );

	// Methods to actually perform the stat() (or lstat() or fstat() )
	int Stat( StatOpType which = STATOP_STAT,
			  bool force = true );
	int Stat( const MyString &path,
			  StatOpType which = STATOP_STAT,
			  bool force = true );
	int Stat( const char *path,
			  StatOpType which = STATOP_STAT,
			  bool force = true );
	int Stat( int fd, bool force = true );	
	int Retry( void );

	// Get the type of most recent operation
	StatOpType GetLastOpType( void ) const
		{ return m_last_which; };

	// Methods to perform get specific results (all default to last op)
	// If previous Stat() operation was _BOTH or _ALL, then
	//  these _LAST will return info on the last of these that was actually
	//  used (for _BOTH: _LSTAT; for _ALL: _FSTAT)
	bool IsBufValid( StatOpType which = STATOP_LAST ) const
		{ return IsBufValid(GetStat(which) ); };
	int GetStatus( StatOpType which = STATOP_LAST ) const
		{ return GetRc( which ); };
	int GetRc( StatOpType which = STATOP_LAST ) const
		{ return GetRc(GetStat(which) ); };
	int GetErrno( StatOpType which = STATOP_LAST ) const
		{ return GetErrno(GetStat(which) ); };
	const char *GetStatFn( StatOpType which = STATOP_LAST ) const
		{ return GetStatFn(GetStat(which) ); };
	const StatStructType *GetBuf( StatOpType which = STATOP_LAST ) const
		{ return GetBuf(GetStat(which) ); };
	bool GetBuf( StatStructType &buf, StatOpType which = STATOP_LAST ) const
		{ return GetBuf(GetStat(which), buf); };

private:
	// Initialize the object
	void init( void );

	// Internal methods to return the expected info
	StatWrapperIntBase *GetStat( StatOpType which ) const;
	
	// Get info from one of the internal stat objects
	int GetErrno( const StatWrapperIntBase * ) const;
	int GetRc( const StatWrapperIntBase * ) const;
	bool IsBufValid( const StatWrapperIntBase * ) const;
	bool IsValid( const StatWrapperIntBase * ) const;
	const StatStructType *GetBuf( const StatWrapperIntBase * ) const;
	bool GetBuf( const StatWrapperIntBase *, StatStructType & ) const;
	const char *GetStatFn( const StatWrapperIntBase * ) const;

	// Given a "which", should we do these operations?
	bool ShouldStat( StatOpType which );
	bool ShouldLstat( StatOpType which );
	bool ShouldFstat( StatOpType which );
	
	// The stat types
	StatWrapperIntNop	*m_nop;
	StatWrapperIntPath	*m_stat;
	StatWrapperIntPath	*m_lstat;
	StatWrapperIntFd	*m_fstat;

	// Which did we do last?
	StatOpType			 m_last_which;
	StatWrapperOp		*m_last_op;
	StatWrapperOp		*m_ops[ STATOP_NUM ];
	StatWrapperIntBase	*m_last_stat;
};

#endif
