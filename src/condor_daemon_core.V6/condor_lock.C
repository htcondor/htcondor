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
#include "condor_daemon_core.h"
#include "condor_lock.h"
#include "condor_lock_file.h"


// Basic CondorLock constructor
CondorLock::CondorLock( const char	*lock_url,
						const char	*lock_name,
						Service		*app_service,
						LockEvent	lock_event_acquired,
						LockEvent	lock_event_lost,
						time_t		poll_period,
						time_t		lock_hold_time,
						bool		auto_refresh ) 
	: CondorLockBase( )
{

		// This should be table driven, but for right now, there's just
		// once choice; the file lock.  If it says no, give up!!


		// First, get the rank.
	if ( CondorLockFile::Rank( lock_url ) <= 0 ) {
		delete( real_lock );
		EXCEPT( "Lock URL unknown '%s'", lock_url );
	}

		// Now, let it build itself
	real_lock = CondorLockFile::Construct( lock_url,
										   lock_name,
										   app_service,
										   lock_event_acquired,
										   lock_event_lost,
										   poll_period,
										   lock_hold_time,
										   auto_refresh );
}

// Destructor
CondorLock::~CondorLock( void )
{
	delete( real_lock );
	free( (void *) lock_url );
}

// Shared initialization code
int
CondorLock::BuildLock( const char	*lock_url,
					   const char	*lock_name )
{
	return real_lock->BuildLock( lock_url, lock_name );
}

// Set period information
int
CondorLock::SetPeriods( time_t	poll_period,
						time_t	lock_hold_time )
{
	return real_lock->SetPeriods( poll_period, lock_hold_time );
}

// Application level "Acquire Lock"
// Return values:
//		 0: Ok, lock aquired
//		 1: Ok, lock busy, not aquired
//		-1: Failure
int
CondorLock::AcquireLock( bool background, int *callback_status )
{
	return real_lock->AcquireLock( background, callback_status );
}

// Application level "Refresh Lock"
// Return values:
//		 0: Ok, lock aquired
//		 1: Ok, lock busy, not aquired
//		-1: Failure, lock lost
int
CondorLock::RefreshLock( int *callback_status )
{
	return real_lock->RefreshLock( callback_status );
}

// Application level "Release Lock"
// Return values:
//		 0: Ok, lock release
//		-1: Failure
int
CondorLock::ReleaseLock( int *callback_status )
{
	return real_lock->ReleaseLock( callback_status );
}

// Application level "Do I have the lock?"
// Return values:
//		 true: Yes
//		false: No
bool
CondorLock::HaveLock( void )
{
	return real_lock->HaveLock( );
}

// Return a simple string describing the event src
const char *
CondorLock::EventSrcString( LockEventSrc src )
{
	if ( LOCK_SRC_APP == src ) {
		return "application";
	} else if ( LOCK_SRC_POLL == src ) {
		return "poll";
	} else {
		return "Invalid";
	}
}
