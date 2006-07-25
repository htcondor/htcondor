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
	int DeleteMe();

	int Ping();
	virtual void DoPing( time_t& ping_delay, bool& ping_complete,
						 bool& ping_succeeded );

	int UpdateLeases();
	virtual void DoUpdateLeases( time_t& update_delay, bool& update_complete,
								 SimpleList<PROC_ID>& update_succeeded );

	char *resourceName;
	int deleteMeTid;
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

	bool hasLeases;
	int updateLeasesTimerId;
	time_t lastUpdateLeases;
	List<BaseJob> leaseUpdates;
	bool updateLeasesActive;
	bool leaseAttrsSynched;
	bool updateLeasesCmdActive;
};

#endif // define BASERESOURCE_H
