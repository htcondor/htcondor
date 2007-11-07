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

#ifndef __CONDOR_LOCK_FILE__
#define __CONDOR_LOCK_FILE__

#include "condor_timer_manager.h"
#include "condor_daemon_core.h"
#include "condor_lock_implementation.h"

// Define a basic "lock file" class
// Derived from "Serice" for Event & EventCpp
class CondorLockFile : public CondorLockImpl
{
  public:
	CondorLockFile( void );
	CondorLockFile( const char	*lock_url,
					const char	*lock_name,
					Service		*app_service,
					LockEvent	lock_event_acquired,
					LockEvent	lock_event_lost,
					time_t		poll_period = 0,
					time_t		lock_hold_time = 0,
					bool		auto_update = false );
	~CondorLockFile( void );

	// Give me the rank for this URL
	static int Rank( const char	*lock_url );

	// Construct a lock & return a pointer to it
	static CondorLockImpl *Construct( const char	*lock_url,
									  const char	*lock_name,
									  Service		*app_service,
									  LockEvent		lock_event_acquired,
									  LockEvent		lock_event_lost,
									  time_t		poll_period = 0,
									  time_t		lock_hold_time = 0,
									  bool			auto_update = false );

	// Actually create the lock (normall called by the constructors)
	int BuildLock( const char	*lock_url,
				   const char	*lock_name );

	int ChangeUrlName( const char *lock_url,
					   const char *lock_name );

  private:
	MyString		lock_url;
	MyString		lock_name;
	MyString		lock_file;
	MyString		temp_file;

	// Private member functions
	int GetLock( time_t lock_hold_time );
	int UpdateLock( time_t lock_hold_time );
	int FreeLock( void );

	// Internal use
	int SetExpireTime( const char *file, time_t lock_hold_time );

};

#endif
