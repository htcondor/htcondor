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


#ifndef BASERESOURCE_H
#define BASERESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"


class BaseJob;

class BaseResource : public Service
{
 public:

	BaseResource( const char *resource_name );
	virtual ~BaseResource();

	virtual void Reconfig();

	virtual const char *ResourceType() = 0;

	char *ResourceName();

	virtual const char *GetHashName() = 0;

	virtual void PublishResourceAd( ClassAd *resource_ad );

	virtual void RegisterJob( BaseJob *job );
	virtual void UnregisterJob( BaseJob *job );
	bool IsEmpty();

	void RequestPing( BaseJob *job );
	bool IsDown();
	time_t getLastStatusChangeTime() { return lastStatusChange; }

	bool RequestSubmit( BaseJob *job );
	void CancelSubmit( BaseJob *job );
	void AlreadySubmitted( BaseJob *job );

    static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

 protected:
	void DeleteMe();

	void Ping();
	virtual void DoPing( time_t& ping_delay, bool& ping_complete,
						 bool& ping_succeeded );

	void UpdateLeases();
	virtual void DoUpdateLeases( time_t& update_delay, bool& update_complete,
								 SimpleList<PROC_ID>& update_succeeded );
	bool Invalidate ();
    bool SendUpdate ();	
	void UpdateCollector ();

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

	// jobs allowed to submit under jobLimit
	List<BaseJob> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<BaseJob> submitsWanted;
	int jobLimit;			// max number of submitted jobs

	bool hasLeases;
	int updateLeasesTimerId;
	time_t lastUpdateLeases;
	List<BaseJob> leaseUpdates;
	bool updateLeasesActive;
	bool leaseAttrsSynched;
	bool updateLeasesCmdActive;

	int _updateCollectorTimerId;
	time_t _lastCollectorUpdate;
	time_t _collectorUpdateInterval;
	bool _firstCollectorUpdate;

};

#endif // define BASERESOURCE_H
