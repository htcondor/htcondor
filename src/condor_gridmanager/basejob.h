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


#ifndef BASEJOB_H
#define BASEJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "user_job_policy.h"
#include "baseresource.h"

class BaseResource;

class BaseJob : public Service
{
 public:
	BaseJob( ClassAd *ad );
	virtual ~BaseJob();

	static void BaseJobReconfig();

	virtual void Reconfig() {}
	void SetEvaluateState() const;
	virtual void doEvaluateState(int timerID);
	virtual BaseResource *GetResource();

	void JobSubmitted( const char *remote_host);
	void JobRunning();
	void JobIdle();
	void JobEvicted();
	void JobTerminated();
	void JobCompleted();
	void DoneWithJob();
	void JobHeld( const char *hold_reason, int hold_code = 0,
				  int hold_sub_code = 0 );
	void JobRemoved( const char *remove_reason );

	virtual void SetRemoteJobId( const char *job_id );
	bool SetRemoteJobStatus( const char *job_status );

	void UpdateJobLeaseSent( time_t new_expiration_time );
	void UpdateJobLeaseReceived( time_t new_expiration_time );

	void SetJobLeaseTimers();
	virtual void JobLeaseSentExpired(int timerID);
	virtual void JobLeaseReceivedExpired(int timerID);

	virtual void JobAdUpdateFromSchedd( const ClassAd *new_ad, bool full_ad );

	static void EvalAllPeriodicJobExprs(int tid);
	int EvalPeriodicJobExpr();
	int EvalOnExitJobExpr();

	static void CheckAllRemoteStatus(int tid);
	static int m_checkRemoteStatusTid;
	void CheckRemoteStatus();

	void UpdateJobTime( float *old_run_time, bool *old_run_time_dirty ) const;
	void RestoreJobTime( float old_run_time, bool old_run_time_dirty ) const;

	virtual void RequestPing();
	virtual void NotifyResourceDown();
	virtual void NotifyResourceUp();

	static std::unordered_map<PROC_ID, BaseJob *> JobsByProcId;
	static std::unordered_map<std::string, BaseJob *> JobsByRemoteId;

	ClassAd *jobAd;
	PROC_ID procID;

	int condorState;

	bool calcRuntimeStats;

	bool writeUserLog;
	bool submitLogged;
	bool executeLogged;
	bool submitFailedLogged;
	bool terminateLogged;
	bool abortLogged;
	bool evictLogged;
	bool holdLogged;

	bool exitStatusKnown;

	bool deleteFromGridmanager;
	bool deleteFromSchedd;

	bool resourceDown;
	bool resourceStateKnown;
	bool resourcePingPending;
	bool resourcePingComplete;

	time_t m_lastRemoteStatusUpdate;
	bool m_currentStatusUnknown;

	int evaluateStateTid;

 protected:
	static int periodicPolicyEvalTid;

	void UpdateRuntimeStats();

	int jobLeaseSentExpiredTid;
	int jobLeaseReceivedExpiredTid;
};

bool WriteExecuteEventToUserLog( ClassAd *job_ad );
bool WriteAbortEventToUserLog( ClassAd *job_ad );
bool WriteTerminateEventToUserLog( ClassAd *job_ad );
bool WriteEvictEventToUserLog( ClassAd *job_ad );
bool WriteHoldEventToUserLog( ClassAd *job_ad );
bool WriteGridResourceUpEventToUserLog( ClassAd *job_ad );
bool WriteGridResourceDownEventToUserLog( ClassAd *job_ad );
bool WriteGridSubmitEventToUserLog( ClassAd *job_ad );
bool WriteJobStatusUnknownEventToUserLog( ClassAd *job_ad );
bool WriteJobStatusKnownEventToUserLog( ClassAd *job_ad );
bool WriteJobStatusUnknownEventToUserLog( ClassAd *job_ad );
bool WriteJobStatusKnownEventToUserLog( ClassAd *job_ad );
void EmailTerminateEvent( ClassAd * job_ad, bool exit_status_known );

#endif // define BASEJOB_H
