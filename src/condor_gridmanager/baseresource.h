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

// This is the shortest time between two runs of UpdateLeases()
#define UPDATE_LEASE_DELAY 30

class BaseJob;
class GahpClient;

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

	// TODO Make this private and provide an accessor function?
	time_t m_sharedLeaseExpiration;

	int m_paramJobPollRate;
	int m_paramJobPollInterval;
	int m_jobPollInterval;

	virtual void SetJobPollInterval();
	int GetJobPollInterval() { return m_jobPollInterval; };

    bool didFirstPing() { return firstPingDone; }

 protected:
	void DeleteMe();

	void Ping();
	virtual void DoPing( time_t& ping_delay, bool& ping_complete,
						 bool& ping_succeeded );

	void UpdateLeases();
	virtual void DoUpdateLeases( time_t& update_delay, bool& update_complete,
								 SimpleList<PROC_ID>& update_succeeded );
	virtual void DoUpdateSharedLease( time_t& update_delay,
									  bool& update_complete,
									  bool& update_succeeded );
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
	bool m_hasSharedLeases;
	time_t m_defaultLeaseDuration;

	int _updateCollectorTimerId;
	time_t _lastCollectorUpdate;
	time_t _collectorUpdateInterval;
	bool _firstCollectorUpdate;

private:

	bool m_batchStatusActive;
	int m_batchPollTid;

protected:
	// After StartBatchStatusTimer is called, return the timer's ID,
	// suitable for passing to GahpClient::setNotificationTimerId.
	// Do not call before calling StartBatchStatusTimer!
	int BatchPollTid() const { 
		ASSERT(m_batchPollTid); 
		return m_batchPollTid;
	}

	// If a subclass wants the batch status timer running, call this function
	// to initialize it.  If you call this, you MUST re-implement
	// BatchStatusInterface, StartBatchStatus, FinishBatchStatus, and BatchGahp
	// Do not call more than once.
	void StartBatchStatusTimer();

	enum BatchStatusResult {
		// All done, next call should be to StartBatchStatus
		// BatchStatus Timer was _not_ reset.
		BSR_DONE,
		// Waiting, next call should be to FinishBatchStatus
		// BatchStatus Timer will be reset to 0 when ready.
		BSR_PENDING,
		// Oops, next call should be to StartBatchStatus
		// BatchStatus Timer was _not_ reset.
		BSR_ERROR,
	};

	// Only called if WatchBatchStatusTimer()
	// How often do you want the batch status probe to run?
	// Called every time the timer fires.
	// Defaults to EXCEPTing!
	virtual int BatchStatusInterval() const;

	// Only called if WatchBatchStatusTimer()
	// Begins a batch status probe.
	// Defaults to EXCEPTing!
	// If this returns BSR_PENDING, this function is
	// responsible for resetting the timer when it's ready!
	virtual BatchStatusResult StartBatchStatus();

	// Only called if WatchBatchStatusTimer()
	// Called repeatedly after StartBatchStatus
	// until returns true.
	// Defaults to EXCEPTing!
	// If this returns BSR_PENDING, this function is
	// responsible for resetting the timer when it's ready!
	virtual BatchStatusResult FinishBatchStatus();

	// Only called if WatchBatchStatusTimer()
	// Return a pointer to the Gahp you need for StartBatchStatus
	// and FinishBatchStatus to work.  If !gahp->isStarted(),
	// we'll try to gahp->Startup().  If Startup fails, we'll
	// skill DoBatchStatus.  You can return 0 to indicate
	// that you don't care. Defaults to EXCEPTing!
	virtual GahpClient * BatchGahp();

	// Only called if WatchBatchStatusTimer()
	// Implements the batch status probe, calling out to
	// StartBatchStatus and FinishBatchStatus to implement.
	int DoBatchStatus();
};

#endif // define BASERESOURCE_H
