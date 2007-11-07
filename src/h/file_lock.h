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

#ifndef FILE_LOCK_H
#define FILE_LOCK_H

typedef enum { READ_LOCK, WRITE_LOCK, UN_LOCK } LOCK_TYPE;

#if defined(__cplusplus)

	// C++ wrapper for lock_file.  Note that the constructor takes 
	// the path to the file - if the path is supplied, then this class
	// will attempt to use a mutex instead of a filesystem lock unless
	// the config file says otherwise.  We do this because (a) filesystem
	// locks are often broken over NFS, and (b) filesystem locks are often
	// not FIFO, so some lock waiters can starve.  -Todd Tannenbaum 7/2007
class FileLock {
public:
	FileLock( int fd, FILE *fp = NULL, const char* path = NULL );
	FileLock( const char* path );
	~FileLock();

		// Set the file descriptor  & pointer
	void SetFdFp( int fd, FILE *fp = NULL );
	
		// Read only access functions
	bool		is_blocking();		// whether or not operations will block
	LOCK_TYPE	get_state();		// cur state READ_LOCK, WRITE_LOCK, UN_LOCK
	void		display();

		// Control functions
	void		set_blocking( bool );	// set blocking TRUE or FALSE
	bool		obtain( LOCK_TYPE t );		// get a lock
	bool		release();					// release a lock

private:
	void Reset( void );

	int			m_fd;		// File descriptor to deal with
	FILE		*m_fp;
	bool		blocking;	// Whether or not to block
	LOCK_TYPE	state;		// Type of lock we are holding
	char*		m_path;		// Path to the file being locked, or NULL
	int			use_kernel_mutex;	// -1=unitialized,0=false,1=true
	int			lock_via_mutex(LOCK_TYPE type);
#ifdef WIN32
	HANDLE		debug_win32_mutex;
#endif

};

#endif	/* cpluscplus */

#endif
