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

		// BuildLock does the real work of building the lock
	BuildLock( lock_url,
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

// Set period information
int
CondorLock::SetPeriods( time_t	poll_period,
						time_t	lock_hold_time,
						bool	auto_refresh )
{
	return real_lock->SetPeriods( poll_period, lock_hold_time, auto_refresh );
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

// Set (change) lock parameters
int
CondorLock::SetLockParams( const char	*lock_url,
						   const char	*lock_name,
						   time_t		poll_period,
						   time_t		lock_hold_time,
						   bool			auto_refresh ) 
{

		// See if the URL or name has changed; if so, we need to rebuild
		// the lock from scratch
	if ( real_lock->ChangeUrlName( lock_url, lock_name ) ) {

			// Delete & rebuild the lock anew
		dprintf( D_ALWAYS,
				 "Lock URL / name incompatibile; rebuilding lock\n" );

			// First, extract the service & handler info from it...
		Service *app_service = real_lock->GetAppService( );
		LockEvent lock_event_acquired = real_lock->GetAcquiredHandler( );
		LockEvent lock_event_lost = real_lock->GetLostHandler( );

			// Now, delete the old lock
		delete( real_lock );

			// Finally, reconstruct the new lock with the new params &
			// old service & handler info
		return BuildLock ( lock_url,
						   lock_name,
						   app_service,
						   lock_event_acquired,
						   lock_event_lost,
						   poll_period,
						   lock_hold_time,
						   auto_refresh );
	}

		// Ok, from here, we should be safe to just set lock parameters
	return ( real_lock->SetPeriods( poll_period,
									lock_hold_time,
									auto_refresh ) );

}

// Do the real work of "building" a lock
int
CondorLock::BuildLock( const char	*lock_url,
					   const char	*lock_name,
					   Service		*app_service,
					   LockEvent	lock_event_acquired,
					   LockEvent	lock_event_lost,
					   time_t		poll_period,
					   time_t		lock_hold_time,
					   bool			auto_refresh )
{

		// This should be table driven, but for right now, there's just
		// once choice; the file lock.  If it says no, give up!!

		// First, get the rank.
	if ( CondorLockFile::Rank( lock_url ) <= 0 ) {
		return -1;
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

		// If the lock construction failed, we've failed...
	if ( ! real_lock ) {
		return -1;
	} else {
		return 0;
	}
}
