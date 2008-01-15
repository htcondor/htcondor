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

#ifndef __CONDOR_LOCK__
#define __CONDOR_LOCK__

#include "condor_lock_base.h"
#include "condor_lock_implementation.h"

// Define a application level "Condor lock" class
class CondorLock : public CondorLockBase
{
  public:
	CondorLock( const char	*lock_url,
				const char	*lock_name,
				Service		*app_service,
				LockEvent	lock_event_acquired,
				LockEvent	lock_event_lost,
				time_t		poll_period = 0,
				time_t		lock_hold_time = 0,
				bool		auto_refresh = false );
	~CondorLock( void );

	// Change lock parameters
	int SetLockParams( const char	*lock_url,
					   const char	*lock_name,
					   time_t		poll_period,
					   time_t		lock_hold_time,
					   bool			auto_refresh = false ) ;

	// Adjust the timing parameters
	virtual int SetPeriods( time_t	poll_period,
							time_t	lock_hold_time,
							bool	auto_refresh = false );

	// Basic lock operations
	virtual int AcquireLock( bool	background,
							 int	*callback_status = NULL );
	virtual int ReleaseLock( int	*callback_status = NULL );
	virtual int RefreshLock( int	*callback_status = NULL );;
	virtual bool HaveLock( void );

	// Return a string which describes the lock src passed to the app
	const char *EventSrcString( LockEventSrc src );

  private:

	// Internal function which builds the lock
	int BuildLock( const char	*lock_url,
				   const char	*lock_name,
				   Service		*app_service,
				   LockEvent	lock_event_acquired,
				   LockEvent	lock_event_lost,
				   time_t		poll_period,
				   time_t		lock_hold_time,
				   bool			auto_refresh );

	// Private data
	CondorLockImpl	*real_lock;

};

#endif
