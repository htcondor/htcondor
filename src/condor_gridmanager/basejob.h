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

#ifndef BASEJOB_H
#define BASEJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "user_log.c++.h"
#include "user_job_policy.h"
#include "baseresource.h"

class BaseResource;

class BaseJob
{
 public:
	BaseJob( ClassAd *ad );
	virtual ~BaseJob();

	virtual void Reconfig() {}
	void SetEvaluateState();
	virtual int doEvaluateState();
	virtual BaseResource *GetResource();

	void UpdateJobAd( const char *name, const char *value );
	void UpdateJobAdInt( const char *name, int value );
	void UpdateJobAdFloat( const char *name, float value );
	void UpdateJobAdBool( const char *name, int value );
	void UpdateJobAdString( const char *name, const char *value );

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

	virtual void JobAdUpdateFromSchedd( const ClassAd *new_ad );

	int EvalPeriodicJobExpr();
	int EvalOnExitJobExpr();

	void UpdateJobTime( float *old_run_time, bool *old_run_time_dirty );
	void RestoreJobTime( float old_run_time, bool old_run_time_dirty );

	virtual void NotifyResourceDown() {}
	virtual void NotifyResourceUp() {}

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

	int wantResubmit;
	int doResubmit;
	int wantRematch;

 protected:
	void UpdateRuntimeStats();

	int periodicPolicyEvalTid;
	int evaluateStateTid;
};

UserLog *InitializeUserLog( ClassAd *job_ad );
bool WriteExecuteEventToUserLog( ClassAd *job_ad );
bool WriteAbortEventToUserLog( ClassAd *job_ad );
bool WriteTerminateEventToUserLog( ClassAd *job_ad );
bool WriteEvictEventToUserLog( ClassAd *job_ad );
bool WriteHoldEventToUserLog( ClassAd *job_ad );
void EmailTerminateEvent( ClassAd * job_ad, bool exit_status_known );

#endif // define BASEJOB_H
