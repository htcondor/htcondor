/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef BASERESOURCE_H
#define BASERESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "basejob.h"

class BaseJob;

class BaseResource
{
 public:

	BaseResource( const char *resource_name );
	virtual ~BaseResource();

	virtual void Reconfig();

	char *ResourceName();

	virtual void RegisterJob( BaseJob *job );
	virtual void UnregisterJob( BaseJob *job );
	bool IsEmpty();

	void RequestPing( BaseJob *job );
	bool IsDown();
	time_t getLastStatusChangeTime() { return lastStatusChange; }

	bool RequestSubmit( BaseJob *job );
	void SubmitComplete( BaseJob *job );
	void CancelSubmit( BaseJob *job );
	void AlreadySubmitted( BaseJob *job );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

 protected:
	int Ping();
	virtual void DoPing( time_t& ping_delay, bool& ping_complete,
						 bool& ping_succeeded );

	char *resourceName;
	List<BaseJob> registeredJobs;
	List<BaseJob> pingRequesters;

	bool resourceDown;
	bool firstPingDone;
	bool pingActive;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;

	static int probeInterval;
	static int probeDelay;

	// jobs that are currently executing a submit
	List<BaseJob> submitsInProgress;
	// jobs that want to submit but can't due to submitLimit
	List<BaseJob> submitsQueued;
	// jobs allowed to submit under jobLimit
	List<BaseJob> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<BaseJob> submitsWanted;
	int submitLimit;		// max number of submit actions
	int jobLimit;			// max number of submitted jobs
};

#endif // define BASERESOURCE_H
