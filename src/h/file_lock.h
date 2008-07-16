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
	// the path to the file, which must be supplied. This class
	// will attempt to use a mutex instead of a filesystem lock unless
	// the config file says otherwise.  We do this because (a) filesystem
	// locks are often broken over NFS, and (b) filesystem locks are often
	// not FIFO, so some lock waiters can starve.  -Todd Tannenbaum 7/2007
class FileLock {
public:
	FileLock( int fd, FILE *fp, const char* path );
	FileLock( const char* path );
	~FileLock();

	// Set the file descriptor, pointer, and file (which may not be NULL)
	void SetFdFpFile( int fd, FILE *fp, const char *file );
	
	// Read only access functions
	bool		is_blocking();		// whether or not operations will block
	LOCK_TYPE	get_state();		// cur state READ_LOCK, WRITE_LOCK, UN_LOCK
	void		display();

		// Control functions
	void		set_blocking( bool );	// set blocking TRUE or FALSE
	bool		obtain( LOCK_TYPE t );		// get a lock
	bool		release();					// release a lock

	// when the object is created or the lock file changed with SetFdFpFile(),
	// this is invoked on the filename to ensure it has a current timestamp.
	// This prevents things like linux's tmpwatch from deleting a valid lock
	// file.
	void		update_lock_timestamp(void);

	// An internal static list of lock objects associated with files is walked
	// and each lock object is told to update the timestamp on its lockfile.
	// This makes it easy for daemoncore to periodically update the timestamps
	// on the lock files regardless of the daemon using it.
	static void		update_all_lock_timestamps(void);

	// used for the static linked list of all valid locks.
	struct FileLockEntry
	{
		FileLock *fl;
		FileLockEntry *next;
	};

private:
	void Reset( void );

	// When the lock object is instantiated, it keeps track of itself in
	// a statically available master list--but only if it has a valid path
	// which can also be adjusted by SetFdFpFile()()
	void record_existence(void);
	void erase_existence(void);

	unsigned int m_id;		// the ID number for this object
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

	// Keep track of all valid FileLock objects allocated anywhere. This
	// allows daemoncore to ask them to touch the atime of their lock files
	// so programs like tmpwatch under linux don't get rid of in use
	// lock files. A constraint is that a filelock is only recorded into
	// this list if it has a non-null filename pointer associated with it.
	// WARNING: I could not use a structure like the HashTable here because
	// this file is included with user_log.c++.h and distributed with
	// libcondorapi. Using HashTable would have caused many dependancies to
	// be brought in header file-wise and things will get very ugly. 
	static FileLockEntry *m_all_locks;
};

#endif	/* cpluscplus */

#endif
