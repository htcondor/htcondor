/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
