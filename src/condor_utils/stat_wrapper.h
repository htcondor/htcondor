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
#include <string>

class StatWrapper
{
public:
	// Constructors & destructors
	// The forms that supply a path or fd will perform a Stat()
	StatWrapper( const char *path, bool do_lstat = false );
	StatWrapper( const std::string &path, bool do_lstat = false );
	StatWrapper( int fd );
	StatWrapper( void );
	~StatWrapper( void );

	// Check to see if we're initialized with a target path or FD
	bool IsInitialized( void ) const;

	// Methods to set the path and FD
	// These calls do not perform a stat operation
	void SetPath( const char *path, bool do_lstat = false );
	void SetFD( int fd );

	// Methods to actually perform the stat() (or lstat() or fstat() )
	int Stat();
	int Stat( const char *path,
			  bool do_lstat = false );
	int Stat( int fd );	

	// Methods to get specific results
	// IsBufValid() returns true if the most recent attempt to stat the
	//   current target path or fd was successful
	bool IsBufValid() const
		{ return m_buf_valid; };
	int GetRc() const
		{ return m_rc; };
	int GetErrno() const
		{ return m_errno; };
	// GetStatFn() returns the name of the actual stat function used
	const char *GetStatFn() const;

	// Get access to the stat buffer
	// Contents are undefined if IsBufValid() returns false
	const StatStructType *GetBuf() const
		{ return &m_statbuf; };

private:
	StatStructType m_statbuf;
	std::string m_path;
	int m_rc;
	int m_errno;
	int m_fd;
	bool m_do_lstat;
	bool m_buf_valid;
};

#endif
