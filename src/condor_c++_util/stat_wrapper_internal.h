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

#ifndef STAT_WRAPPER_INTERNAL_H
#define STAT_WRAPPER_INTERNAL_H

#include "stat_struct.h"

// StatWrapper internal -- base class
class StatWrapperIntBase
{
public:
	StatWrapperIntBase( const char *name );
	StatWrapperIntBase( const StatWrapperIntBase & );
	virtual ~StatWrapperIntBase( void ) { };

	// Copy another object
	void Copy( const StatWrapperIntBase & );

	// Do a stat on the same file / fd
	virtual int Stat( bool force ) = 0;

	// Simple accessors
	bool IsBufValid( void ) const
		{ return m_buf_valid; };
	int GetRc( void ) const
		{ return m_rc; };
	int GetErrno( void ) const
		{ return m_errno; };
	const StatStructType *GetBuf( void ) const
		{ if (! m_valid) { return NULL; } return &m_buf; };
	bool GetBuf( StatStructType &buf ) const
		{ buf = m_buf; return m_valid; };
	const char *GetFnName( void ) const
		{ return m_name; };
	virtual bool IsValid( void ) const
		{ return m_valid; };

protected:
	int CheckResult( void );
	int SetRc( int rc )
		{ return m_rc = rc; };

	StatStructType	m_buf;			// Stat buffer
	bool			m_buf_valid;	// Is the above valid?
	bool			m_valid;		// Is path/fd valid?
	const char		*m_name;		// Name of the function used
	int				m_rc;			// return code
	int				m_errno;		// errno
};

// StatWrapper internal -- Path Version
class StatWrapperIntPath : public StatWrapperIntBase
{
public:
	StatWrapperIntPath( const char *name,
						int (* const fn)(const char *, StatStructType *) );
	StatWrapperIntPath( const StatWrapperIntPath &other );
	~StatWrapperIntPath( void );

	// Type of the stat function
	typedef int ( * StatFnPtr ) ( const char *, StatStructType * );
	
	// Copy another object
	void Copy( const StatWrapperIntPath & );

	// Set the path
	bool SetPath( const char *);

	// Do the stat
	int Stat( bool force );

	// Accessors
	const StatFnPtr  GetFn  ( void ) const { return m_fn; };
	const char      *GetPath( void ) const { return m_path; };

private:
	StatFnPtr	m_fn;
	const char *m_path;

};

// StatWrapper internal -- FD Version
class StatWrapperIntFd : public StatWrapperIntBase
{
public:
	StatWrapperIntFd( const char *name,
					  int (* const fn)(int, StatStructType *) );
	StatWrapperIntFd( const StatWrapperIntFd &other );
	~StatWrapperIntFd( void );

	// Type of the stat function
	typedef int ( * StatFnPtr ) ( int, StatStructType * );

	// Copy another object
	void Copy( const StatWrapperIntFd & );

	// Set the FD
	bool SetFD( int fd );

	// Do the stat
	int Stat( bool force );

	// Accessors
	const StatFnPtr GetFn( void ) const { return m_fn; };
	int             GetFd( void ) const { return m_fd; };

private:
	StatFnPtr	m_fn;
	int			m_fd;
};

// StatWrapper internal -- NOP Version
class StatWrapperIntNop : public StatWrapperIntBase
{
public:
	StatWrapperIntNop( const char *name,
					   int (* const fn)(int, StatStructType *) );
	StatWrapperIntNop( const StatWrapperIntNop &other );
	~StatWrapperIntNop( void );

	// Type of the stat function
	typedef int ( * StatFnPtr ) ( int, StatStructType * );

	// Copy another object
	void Copy( const StatWrapperIntNop & );

	// Do the stat
	int Stat( bool force );

	// Accessors
	const StatFnPtr GetFn( void ) const { return m_fn; };

private:
	StatFnPtr	m_fn;
};


#endif
