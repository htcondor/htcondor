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
