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
#include "condor_daemon_core.h"
#include "condor_lock_implementation.h"


// Basic CondorLockImpl constructor
CondorLockImpl::CondorLockImpl( void )
{
		// Set the event handler
	this->app_service = NULL;
	this->lock_event_acquired = (LockEvent) NULL;
	this->lock_event_lost = (LockEvent) NULL;

		// Do the rest of the initializations
	Init( 0, 0, false );
}

// Basic CondorLockImpl constructor
CondorLockImpl::CondorLockImpl( Service		*ap_service,
								LockEvent	le_acquired,
								LockEvent	le_lost,
								time_t		poll_per,
								time_t		lock_ht,
								bool		auto_re )
{
		// Quick sanity check
	if ( (!ap_service) && ( le_acquired || le_lost ) ) {
		EXCEPT( "CondorLockImpl constructed with c++ pointer "
				"and NULL Service!\n" );
	}

		// Set the event handler
	this->app_service = ap_service;
	this->lock_event_acquired = le_acquired;
	this->lock_event_lost = le_lost;

		// Do the rest of the initializations
	Init( poll_per, lock_ht, auto_re );
}

// Shared initialization code
int
CondorLockImpl::Init( time_t		poll_per,
					  time_t		lock_ht,
					  bool			auto_re )
{
		// Initialize some stuff
	this->timer = -1;
	this->last_poll = 0;
	this->have_lock = false;
	this->lock_enabled = false;

		// Force timing stuff to known state
	this->poll_period = 0;
	this->old_poll_period = 0;
	this->lock_hold_time = 0;
	this->auto_refresh = false;

		// Store off lock periods, etc
	return SetPeriods( poll_per, lock_ht, auto_re );

		// Done
	return 0;
}

// Destructor
CondorLockImpl::~CondorLockImpl( void )
{
		// Lock lost; notify application
	if ( have_lock ) {
		ReleaseLock();
	}

		// Free up the timer
	if ( timer >= 0 ) {
		daemonCore->Cancel_Timer( timer );
	}
}

// Implement the lock internals
int
CondorLockImpl::ImplementLock( void )
{
		// Start the timer
	return SetupTimer( );
}

// Set period information
int
CondorLockImpl::SetPeriods( time_t	poll_per,
							time_t	lock_ht,
							bool	auto_re )
{
		// Note if some things have changed....
	bool	hold_changed = ( this->lock_hold_time != lock_ht );

		// Store the new parameters
	this->poll_period = poll_per;
	this->lock_hold_time = lock_ht;
	this->auto_refresh = auto_re;

		// Refresh the lock
	if ( have_lock && hold_changed && auto_re ) {
		int status = UpdateLock( lock_ht );
		if ( status ) {
			(void) LockLost( LOCK_SRC_POLL );
		}
	}

		// Finally, kick off the timer
	return SetupTimer( );
}

// Handle new timer reltated configuration
int
CondorLockImpl::SetupTimer( void )
{
		// If the poll period hasn't changed, nothing to do here...
	if ( poll_period == old_poll_period ) {
		return 0;
	}

		// If the poll period is zero, not much to do...
	if ( 0 == poll_period ) {
		last_poll = 0;

			// Free up the timer
		if ( timer >= 0 ) {
			daemonCore->Cancel_Timer( timer );
		}

			// Done
		old_poll_period = poll_period;
		return 0;
	}

		// Figure out when then next poll should be, etc...
	time_t	now = time( NULL );
	time_t	next_poll;
	if ( last_poll ) {
		next_poll = last_poll + poll_period;
	} else {
		next_poll = now + poll_period;
	}

		// If the timer already exists, kill it
	if ( timer >= 0 ) {
		daemonCore->Cancel_Timer( timer );
		timer = -1;
	}

		// Is it's time up?
	if ( last_poll && ( last_poll <= now ) ) {
		DoPoll( );
	}

		// Create the timer...
	timer = daemonCore->Register_Timer(
			next_poll - now,
			poll_period,
			(TimerHandlercpp) &CondorLockImpl::DoPoll,
			"CondorLockImpl",
			this );
	if ( timer < 0 ) {
		dprintf( D_ALWAYS, "CondorLockImpl: Failed to create timer\n" );
		return -1;
	}

		// All done now
	return 0;
}

// Application level "Acquire Lock"
// Return values:
//		 0: Ok, lock aquired
//		 1: Ok, lock busy, not aquired
//		-1: Failure
int
CondorLockImpl::AcquireLock( bool background, int *callback_status )
{
	lock_enabled = true;

		// If we already have the lock, do nothing
	if ( have_lock ) {
		return 0;
	}

		// Try to get the lock
	int		status = GetLock( lock_hold_time );

		// If it succeeded, notify the application
	if ( 0 == status ) {
		int		cb = LockAcquired( LOCK_SRC_APP );
		if ( callback_status ) {
			*callback_status = cb;
		}
		return 0;
	} else if ( status < 0 ) {
		lock_enabled = false;
		return status;
	} else if ( ! background ) {
		return 1;
	}

		// Return lock busy status
	return 1;
}

// Application level "Refresh Lock"
// Return values:
//		 0: Ok, lock aquired
//		 1: Ok, lock busy, not aquired
//		-1: Failure, lock lost
int
CondorLockImpl::RefreshLock( int *callback_status )
{
	int		status = 0;

		// Verify that we *have* the lock
	if ( ! have_lock ) {
		return -1;
	}

		// And, refresh it
	status = UpdateLock( lock_hold_time );

		// Invoke the callback if the lock has been broken
	int cbstatus = 0;
	if ( status ) {
		cbstatus = LockLost( LOCK_SRC_APP );
	}

		// And, store off the callback status.
	if ( callback_status ) {
		*callback_status = cbstatus;
	}

	return 0;
}

// Application level "Release Lock"
// Return values:
//		 0: Ok, lock release
//		-1: Failure
int
CondorLockImpl::ReleaseLock( int *callback_status )
{
		// Try to get the lock...
	lock_enabled = false;

		// If we don't have the lock, nothing to do!
	if ( ! have_lock ) {
		dprintf( D_FULLDEBUG, "ReleaseLock: we don't own the lock; done\n" );
		return 0;
	}

		// Try to get the lock
	dprintf( D_FULLDEBUG, "ReleaseLock: Freeing the lock\n" );
	int		status = FreeLock( );

		// Notify the application
	int		cb = LockLost( LOCK_SRC_APP );
	if ( callback_status ) {
		*callback_status = cb;
	}

	return status;
}

// Application level "Do I have the lock?"
// Return values:
//		 true: Yes
//		false: No
bool
CondorLockImpl::HaveLock( void )
{
	return have_lock;
}

// Perform poll cycle
void
CondorLockImpl::DoPoll( int /* timerID */ )
{
		// Store the time
	last_poll = time( NULL );

		// Already have the lock; nothing to do.
	if ( have_lock ) {
		if ( auto_refresh ) {
			int status = UpdateLock( lock_hold_time );
			if ( status ) {
				(void) LockLost( LOCK_SRC_POLL );
			}
		}
		return;
	}

		// If we're not trying to acquire the lock, nothing to do
	if ( ! lock_enabled ) {
		return;
	}

		// Ok, now try to grab the lock
	int status = GetLock( lock_hold_time );

		// If it succeeded, notify the application
	if ( 0 == status ) {
		LockAcquired( LOCK_SRC_POLL );
	}
}

// Lock acquired; notify application
int
CondorLockImpl::LockAcquired( LockEventSrc event_src )
{
	int		status = 0;

		// Note that we have the lock
	have_lock = true;

		// Invoke the callback
	if ( lock_event_acquired ) {
		status = (app_service->*lock_event_acquired)( event_src );
	}

		// Done
	return status;
}

// Lock lost; notify application
int
CondorLockImpl::LockLost( LockEventSrc event_src )
{
	int		status = 0;

		// Note that we have the lock
	have_lock = false;

		// Invoke the callback
	if ( lock_event_lost ) {
		status = (app_service->*lock_event_lost)( event_src );
	}

		// Done
	return status;
}
