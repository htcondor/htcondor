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
	const char		*lock_url;
	const char		*lock_name;

};

#endif
