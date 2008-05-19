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

#include "condor_constants.h"

typedef enum { READ_LOCK, WRITE_LOCK, UN_LOCK } LOCK_TYPE;

#if defined(__cplusplus)
extern "C" {
#endif

/* Lock/unlock a file using the OS's file locking mechanism.
 *
 * lock_file_plain() doesn't call any Condor code. It can be used anywhere
 *   in Condor without fear of recursion.
 *
 * lock_file() makes calls to low-level Condor code like param() and
 *   dprintf() to provide additional functionality (like optionally
 *   ignoring locking failures on NFS).
 */

int lock_file( int fd, LOCK_TYPE lock_type, BOOLEAN do_block );
int lock_file_plain( int fd, LOCK_TYPE lock_type, BOOLEAN do_block );

#if defined(__cplusplus)
}		/* End of extern "C" declaration */
#endif

#if defined(__cplusplus)

	// C++ wrapper for lock_file.  Note that the constructor takes 
	// the path to the file - if the path is supplied, then this class
	// will attempt to use a mutex instead of a filesystem lock unless
	// the config file says otherwise.  We do this because (a) filesystem
	// locks are often broken over NFS, and (b) filesystem locks are often
	// not FIFO, so some lock waiters can starve.  -Todd Tannenbaum 7/2007
class FileLockBase
{
  public:
	FileLockBase( void )
		{ m_state = UN_LOCK, m_blocking = true; };
	virtual ~FileLockBase(void) { };

		// Read only access functions
	bool isBlocking(void) const			// whether or not operations will block
		{ return m_blocking; };
	bool isLocked(void) const			// false if state == UN_LOCK
		{ return ( UN_LOCK != m_state ); };
	bool isUnlocked(void) const			// true if state == UN_LOCK
		{ return ( UN_LOCK == m_state ); };
	LOCK_TYPE	getState(void) const	// cur state {READ,WRITE,UN}_LOCK
		{ return m_state; };
	virtual bool isFakeLock(void) const = 0;	// true if this is a fake lock

		// Control functions
	virtual bool obtain( LOCK_TYPE t ) = 0;		// get a lock
	virtual bool release(void) = 0;				// release a lock
	virtual void SetFdFp( int fd, FILE *fp = NULL ) = 0;

		// Set lock mode
	void setBlocking( bool val )		// set blocking TRUE or FALSE
		{ m_blocking = val; };
	
		// Read only access functions
	virtual void display(void) const = 0;
	const char *getStateString( LOCK_TYPE state ) const;

  protected:
	bool		m_blocking;				// Whether or not to block
	LOCK_TYPE	m_state;				// Type of lock we are holding
};

class FileLock : public FileLockBase
{
  public:
	FileLock( int fd, FILE *fp = NULL, const char* path = NULL );
	FileLock( const char* path );
	~FileLock(void);

		// Not a fake lock
	bool isFakeLock(void) const { return false; };
	
		// Set the file descriptor  & pointer
	void SetFdFp( int fd, FILE *fp = NULL );

		// Control functions
	bool obtain( LOCK_TYPE t );		// get a lock
	bool release(void);				// release a lock

		// Read only access functions
	virtual void display(void) const;

  private:
	int			 m_fd;			// File descriptor to deal with
	FILE		*m_fp;
	char		*m_path;		// Path to the file being locked, or NULL
	int			 m_use_kernel_mutex;	// -1=unitialized,0=false,1=true

	// Private methods
	void		Reset( void );
	int			lock_via_mutex( LOCK_TYPE type );
# ifdef WIN32
	HANDLE		m_debug_win32_mutex;
# endif
};

class FakeFileLock : public FileLockBase
{
  public:
	FakeFileLock( void ) : FileLockBase( ) { };
	FakeFileLock( const char* path ) : FileLockBase( ) { };
	~FakeFileLock( void ) { };

		// Is a fake lock
	bool isFakeLock(void) const { return true; };

		// Set the file descriptor  & pointer
	void SetFdFp( int fd, FILE *fp = NULL )
		{ (void) fd; (void) fp; };

		// Read only access functions
	void display(void) const
		{ };

		// Control functions
	bool obtain( LOCK_TYPE t )			// obtain the lock
		{ m_state = t; return true; };
	bool release( void )				// release the lock
		{ m_state = UN_LOCK; return true; };

  private:

};

#endif	/* cpluscplus */

#endif
