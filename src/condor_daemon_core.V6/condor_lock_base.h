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

#ifndef __CONDOR_LOCK_BASE__
#define __CONDOR_LOCK_BASE__


#include "condor_timer_manager.h"
#include "condor_daemon_core.h"

// Lock events
typedef enum { LOCK_SRC_APP, LOCK_SRC_POLL } LockEventSrc;

/// Service Method that returns an int (C++ Version).
typedef int     (Service::*LockEvent)( LockEventSrc );

// Define a base "Condor lock" class
// Derived from "Serice" for the timer
class CondorLockBase : public Service
{
  public:
	CondorLockBase( void );
	~CondorLockBase( void );

	// Adjust the peeriods
	virtual int SetPeriods( time_t	poll_period,
							time_t	lock_hold_time,
							bool	auto_refresh ) = 0;

	// Basic lock operations
	virtual int AcquireLock( bool	background,
							 int	*callback_status = NULL ) = 0;
	virtual int ReleaseLock( int	*callback_status = NULL ) = 0;
	virtual int RefreshLock( int	*callback_status = NULL ) = 0;
	virtual bool HaveLock( void ) = 0;

  private:

};

#endif
