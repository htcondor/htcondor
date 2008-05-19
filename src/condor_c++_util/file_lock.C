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

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "file_lock.h"
#include "utc_time.h"

extern "C" int lock_file( int fd, LOCK_TYPE type, BOOLEAN do_block );

const char *
FileLockBase::getStateString( LOCK_TYPE state ) const
{
	switch( state ) {
	case READ_LOCK:
		return "READ";
	case WRITE_LOCK:
		return "WRITE";
	case UN_LOCK:
		return "UNLOCKED";
	default:
		return "UNKNOWN";
	}
}

FileLock::FileLock( int fd, FILE *fp_arg, const char* path )
		: FileLockBase( )
{
	Reset( );
	m_fd = fd;
	m_fp = fp_arg;
	if (path) {
		m_path = strdup(path);
	}
}

FileLock::FileLock( const char *path )
		: FileLockBase( )
{
	Reset( );
	if (path) {
		m_path = strdup(path);
	}
}

FileLock::~FileLock( void )
{
	if( m_state != UN_LOCK ) {
		release();
	}
	m_use_kernel_mutex = -1;
#ifdef WIN32
	if (m_debug_win32_mutex) {
		::CloseHandle(m_debug_win32_mutex);
		m_debug_win32_mutex = NULL;
	}
#endif
	if ( m_path) free(m_path);
}

void
FileLock::Reset( void )
{
	m_fd = -1;
	m_fp = NULL;
	m_blocking = true;
	m_state = UN_LOCK;
	m_path = NULL;
	m_use_kernel_mutex = -1;
#ifdef WIN32
	m_debug_win32_mutex = NULL;
#endif
}

void
FileLock::SetFdFp( int fd, FILE *fp )
{
	m_fd = fd;
	m_fp = fp;
}

void
FileLock::display( void ) const
{
	dprintf( D_FULLDEBUG, "fd = %d\n", m_fd );
	dprintf( D_FULLDEBUG, "blocking = %s\n", m_blocking ? "TRUE" : "FALSE" );
	dprintf( D_FULLDEBUG, "state = %s\n", getStateString( m_state ) );
}

int
FileLock::lock_via_mutex(LOCK_TYPE type)
{
	(void) type;
	int result = -1;

#ifdef WIN32	// only implemented on Win32 so far...
	char * filename = NULL;
	int filename_len;
	char *ptr = NULL;
	char mutex_name[MAX_PATH];


		// If we made it here, we want to use a kernel mutex.
		//
		// We use a kernel mutex by default to fix a major shortcoming
		// with using Win32 file locking: file locking on Win32 is
		// non-deterministic.  Thus, we have observed processes
		// starving to get the lock.  The Win32 mutex object,
		// on the other hand, is FIFO --- thus starvation is avoided.

		// first, open a handle to the mutex if we haven't already
	if ( m_debug_win32_mutex == NULL && m_path ) {
			// Create the mutex name based upon the lock file
			// specified in the config file.  				
		char * filename = strdup(m_path);
		filename_len = strlen(filename);
			// Note: Win32 will not allow backslashes in the name, 
			// so get rid of em here.
		ptr = strchr(filename,'\\');
		while ( ptr ) {
			*ptr = '/';
			ptr = strchr(filename,'\\');
		}
			// The mutex name is case-sensitive, but the NTFS filesystem
			// is not.  So to avoid user confusion, strlwr.
		strlwr(filename);
			// Now, we pre-append "Global\" to the name so that it
			// works properly on systems running Terminal Services
		_snprintf(mutex_name,MAX_PATH,"Global\\%s",filename);
		free(filename);
		filename = NULL;
			// Call CreateMutex - this will create the mutex if it does
			// not exist, or just open it if it already does.  Note that
			// the handle to the mutex is automatically closed by the
			// operating system when the process exits, and the mutex
			// object is automatically destroyed when there are no more
			// handles... go win32 kernel!  Thus, although we are not
			// explicitly closing any handles, nothing is being leaked.
			// Note: someday, to make BoundsChecker happy, we should
			// add a dprintf subsystem shutdown routine to nicely
			// deallocate this stuff instead of relying on the OS.
		m_debug_win32_mutex = ::CreateMutex(0,FALSE,mutex_name);
	}

		// now, if we have mutex, grab it or release it as needed
	if ( m_debug_win32_mutex ) {
		if ( type == UN_LOCK ) {
				// release mutex
			ReleaseMutex(m_debug_win32_mutex);
			result = 0;	// 0 means success
		} else {
				// grab mutex
				// block 10 secs if do_block is false, else block forever
			result = WaitForSingleObject(m_debug_win32_mutex, 
				m_blocking ? INFINITE : 10 * 1000);	// time in milliseconds
				// consider WAIT_ABANDONED as success so we do not EXCEPT
			if ( result==WAIT_OBJECT_0 || result==WAIT_ABANDONED ) {
				result = 0;
			} else {
				result = -1;
			}
		}

	}
#endif

	return result;
}


bool
FileLock::obtain( LOCK_TYPE t )
{
// lock_file uses lseeks in order to lock the first 4 bytes of the file on NT
// It DOES properly reset the lseek version of the file position, but that is
// not the same (in some very inconsistent and weird ways) as the fseek one,
// so if the user has given us a FILE *, we need to make sure we don't ruin
// their current position.  The lesson here is don't use fseeks and lseeks
// interchangeably...
	int		status = -1;

	if ( m_use_kernel_mutex == -1 ) {
		m_use_kernel_mutex = param_boolean_int("FILE_LOCK_VIA_MUTEX", TRUE);
	}

		// If we have the path, we can try to lock via a mutex.  
	if ( m_path && m_use_kernel_mutex ) {
		status = lock_via_mutex(t);
	}

		// We cannot lock via a mutex, or we tried and failed.
		// Try via filesystem lock.
	if ( status < 0) {
		long lPosBeforeLock = 0;
		if (m_fp) // if the user has a FILE * as well as an fd
		{
			// save their FILE*-based current position
			lPosBeforeLock = ftell(m_fp); 
		}
		
		status = lock_file( m_fd, t, m_blocking );
		
		if (m_fp)
		{
			// restore their FILE*-position
			fseek(m_fp, lPosBeforeLock, SEEK_SET); 	
		}
	}

	if( status == 0 ) {
		m_state = t;
	}
	if ( status != 0 ) {
		dprintf( D_ALWAYS, "FileLock::obtain(%d) failed - errno %d (%s)\n",
	                t, errno, strerror(errno) );
	}
	else {
		UtcTime	now( true );
		dprintf( D_FULLDEBUG,
				 "FileLock::obtain(%d) - @%.6f lock on %s now %s\n",
				 t, now.combined(), m_path, getStateString(t) );
	}
	return status == 0;
}

bool
FileLock::release(void)
{
	return obtain( UN_LOCK );
}
