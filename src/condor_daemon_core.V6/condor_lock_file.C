/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "directory.h"
#include "condor_daemon_core.h"
#include "condor_timer_manager.h"
#include "condor_lock_file.h"

// Check URL static method
int
CondorLockFile::Rank( const char *lock_url )
{
		// Identify it

		// ** This is a quick hack & probably should be improved **
	if ( strncmp( lock_url, "file:", 5 ) ) {
		dprintf( D_FULLDEBUG, "CondorLockFile: '%s': Not a file URL\n",
				 lock_url );
		return 0;		// We don't handle this lock
	}

		// Verify that it's a valid directory
	const char	*dirpath = (lock_url + 5);
	StatInfo	statinfo( dirpath );
	if ( statinfo.Error() ) {
		dprintf( D_FULLDEBUG, "CondorLockFile: '%s' does not exist\n",
				 dirpath );
		return 0;		// We don't handle this lock
	}
	if ( ! statinfo.IsDirectory() ) {
		dprintf( D_FULLDEBUG, "CondorLockFile: '%s' is not a directory\n",
				 dirpath );
		return 0;		// We don't handle this lock
	}

	// Note: We probably should verify that we can write to this directory

		// I like it
	return 100;
}

// Build a file-based lock
CondorLockImpl *
CondorLockFile::Construct( const char	*lock_url,
						   const char	*lock_name,
						   Service		*app_service,
						   LockEvent	lock_event_acquired,
						   LockEvent	lock_event_lost,
						   time_t		poll_period,
						   time_t		lock_hold_time,
						   bool			auto_update )
{
	CondorLockImpl	*newlock = new CondorLockFile(
		lock_url, lock_name, app_service, lock_event_acquired,
		lock_event_lost, poll_period, lock_hold_time, auto_update );
	return newlock;
}

// Basic CondorLockFile constructor
CondorLockFile::CondorLockFile( )
		: CondorLockImpl( )
{
	// Do nothing
}

// Basic CondorLockFile constructor
CondorLockFile::CondorLockFile( const char	*lock_url,
								const char	*lock_name,
								Service		*app_service,
								LockEvent	lock_event_acquired,
								LockEvent	lock_event_lost,
								time_t		poll_period,
								time_t		lock_hold_time,
								bool		auto_update )
		: CondorLockImpl( app_service, lock_event_acquired, lock_event_lost,
						  poll_period, lock_hold_time, auto_update )
{
	if ( BuildLock( lock_url, lock_name ) ) {
		EXCEPT( "Error building lock for URL '%s'", lock_url );
	}
}

// Destructor
CondorLockFile::~CondorLockFile( void )
{
	(void) FreeLock( );
}

// Shared initialization code
int
CondorLockFile::BuildLock( const char	*lock_url,
						   const char	*lock_name )
{
		// Verify the rank
	if ( Rank( lock_url ) <= 0 ) {
		return -1;
	}

		// Copy the URL & name out
	this->lock_url = lock_url;
	this->lock_name = lock_url;

		// Extract the path
	file_path.sprintf( "%s/%s.lock", lock_url + 5, lock_name );

		// Build a temporary file name
	char	hostname[128];
	if ( gethostname( hostname, sizeof( hostname ) ) ) {
		sprintf( hostname, "unknown-%d", rand( ) );
	}
	temp_file.sprintf( "%s.%s-%d", file_path.Value(), hostname, getpid( ) );

	dprintf( D_FULLDEBUG, "HA Lock Init: lock file='%s'\n",
			 file_path.Value() );
	dprintf( D_FULLDEBUG, "HA Lock Init: temp file='%s'\n",
			 temp_file.Value() );

		// Build the lock internals
	return ImplementLock( );
}

// Internal "get lock"
// Return values:
//		 0: Ok, lock aquired
//		 1: Ok, lock busy, not aquired
//		-1: Failure
int
CondorLockFile::GetLock( time_t lock_hold_time )
{
	int				status;

		// Step 1: Stat the lock file
	struct stat		statbuf;
	status = stat( file_path.Value( ), &statbuf );
	if ( 0 == status ) {
		time_t	expire = statbuf.st_mtime;
		time_t	now = time( NULL );

			// If the lock has expired, kill it!
		if ( now >= expire ) {
			dprintf( D_ALWAYS,
					 "GetLock warning: Expired lock found '%s'\n",
					 file_path.Value( ) );

				// Expired lock found, remove it
			status = unlink( file_path.Value( ) );

				// Ok if somebody else killed it
			if ( status && ( ENOENT != errno ) ) {
				dprintf( D_ALWAYS,
						 "GetLock warning: Error expiring lock: %d %s\n",
						 errno, strerror( errno ) );
			}
		} else {
			return 1;
		}

		// ENOENT is ok, but other errors are bad
	} else if ( ENOENT != errno ) {
		dprintf( D_FULLDEBUG,
				 "GetLock: Error stating lock file '%s': %d %s\n",
				 file_path.Value( ), errno, strerror( errno ) );
		return -1;
	}

		// At this point in time, we believe that the lock file
		// does not exist.  Proceed.

		// Step 2: Create the temporary file
	int		fd = creat( temp_file.Value( ), S_IRWXU );
	if ( fd < 0 ) {
		dprintf( D_ALWAYS,
				 "GetLock: Error creating temp lock file '%s': %d %s\n",
				 temp_file.Value( ), errno, strerror( errno ) );
		return -1;
	}
	close( fd );

		// Step 3: Set the expiration time
	status = SetExpireTime( temp_file.Value( ), lock_hold_time );
	if ( status ) {
		dprintf( D_ALWAYS, "GetLock: Error setting expiration time" );
		unlink( temp_file.Value( ) );
		return -1;
	}

		// Step 4: Run the link race...
	status = link( temp_file.Value( ), file_path.Value( ) );
	(void) unlink( temp_file.Value( ) );

		// Now, let's see how our car finished
	if ( 0 == status ) {				// We won!
		return 0;
	} else if ( EEXIST == errno ) {		// Somebody else won the race
		dprintf( D_FULLDEBUG, "GetLock: Lock held by somebody else\n",
				 status );
		return 1;
	} else {							// Some other error occurred
		dprintf( D_ALWAYS,
				 "GetLock: Error linking '%s' to lock file '%s': %d %s\n",
				 temp_file.Value(), file_path.Value( ),
				 errno, strerror( errno ) );
		return -1;
	}
}

// Update the lock's timestamp
int
CondorLockFile::UpdateLock( time_t lock_hold_time )
{
	return SetExpireTime( file_path.Value( ), lock_hold_time );
}

// Release the lock
int
CondorLockFile::FreeLock( void )
{
	int		status = unlink( file_path.Value( ) );

	if ( status ) {
		dprintf( D_ALWAYS, "UpdateLock: Error unlink lock '%s': %d %s\n",
				 file_path.Value(), errno, strerror( errno ) );
		return -1;
	}

	return 0;
}

// Update the lock's timestamp
int
CondorLockFile::SetExpireTime( const char *file, time_t lock_hold_time )
{

	time_t			expire = time( NULL ) + lock_hold_time;
	struct utimbuf	timebuf;

		// First, update the file
	timebuf.actime = expire;
	timebuf.modtime = expire;
	int		status = utime( file, &timebuf );
	if ( status ) {
		dprintf( D_ALWAYS, "UpdateLock: Error updating '%s': %d %s\n",
				 file, errno, strerror( errno ) );
		return -1;
	}

		// Stat it to verify
	struct stat		statbuf;
	status = stat( file, &statbuf );
	if ( 0 != status ) {
		dprintf( D_ALWAYS,
				 "UpdateLock: Error stating lock file '%s': %d %s\n",
				 file_path.Value(), errno, strerror( errno ) );
		return -1;
	}
	if ( statbuf.st_mtime != expire ) {
		dprintf( D_ALWAYS,
				 "UpdateLock: lock file '%s' utime wrong (%d != %d)\n",
				 file, expire, statbuf.st_mtime );
		return -1;
	}

		// All done
	return 0;
}
