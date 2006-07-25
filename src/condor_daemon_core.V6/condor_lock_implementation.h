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
#ifndef __CONDOR_LOCK_IMPLEMENTATION__
#define __CONDOR_LOCK_IMPLEMENTATION__

#include "condor_timer_manager.h"
#include "condor_daemon_core.h"
#include "condor_lock_base.h"


// Define a base "Condor Lock Implenation" class
// Derived from "Serice" for the timer
class CondorLockImpl : public CondorLockBase
{
  public:
	CondorLockImpl( void );
	CondorLockImpl( Service		*app_service,
					LockEvent	lock_event_acquired,
					LockEvent	lock_event_lost,
					time_t		poll_period = 0,
					time_t		lock_hold_time = 0,
					bool		auto_refresh = false );
	~CondorLockImpl( void );

	// Adjust the peeriods
	virtual int SetPeriods( time_t	poll_period,
							time_t	lock_hold_time,
							bool	auto_refresh );

	// Basic lock operations
	virtual int AcquireLock( bool	background,
							 int	*callback_status = NULL );
	virtual int ReleaseLock( int	*callback_status = NULL );
	virtual int RefreshLock( int	*callback_status = NULL );
	virtual bool HaveLock( void );

	// To rebuild a lock, we need to get the service, etc. info
	// These methods allow that; that's the only reason they're here!!
	Service *GetAppService( void ) { return app_service; };
	LockEvent GetAcquiredHandler( void ) { return lock_event_acquired; };
	LockEvent GetLostHandler( void ) { return lock_event_lost; };

	// Do the lock implementation; called by the actual implementation
	int ImplementLock( void );

	// Low level function to change the URL / name of the lock
	//  Returns:
	//   0: No change / ok
	//  -1: Fatal error
	//   1: Can't change w/o rebuilding
	virtual int ChangeUrlName( const char *url, const char *name ) = 0;

  private:
	time_t			poll_period;
	time_t			old_poll_period;
	time_t			lock_hold_time;

	Service			*app_service;
	LockEvent		lock_event_acquired;
	LockEvent		lock_event_lost;

	int				timer;
	bool			auto_refresh;

	time_t			last_poll;
	bool			have_lock;
	bool			lock_enabled;

	// Private member functions
	int Init( time_t		poll_period,
			  time_t		lock_hold_time,
			  bool			auto_refresh );
	int DoPoll( void );
	int SetupTimer( void );

	// Update state & invoke callback
	int LockAcquired( LockEventSrc event_src );
	int LockLost( LockEventSrc event_src );

	// Low level get / release lock functions
	// These are implemented only by the *real* locks.
	virtual int GetLock( time_t lock_hold_time ) = 0;
	virtual int UpdateLock( time_t lock_hold_time ) = 0;
	virtual int FreeLock( void ) = 0;
};

#endif
