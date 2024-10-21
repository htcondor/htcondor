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



#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "spooled_job_files.h"
#include "write_user_log.h"
#include "condor_string.h"

#include "gridmanager.h"
#include "basejob.h"
#include "condor_config.h"
#include "condor_email.h"
#include "classad_helpers.h"
#include "classad_merge.h"
#include "condor_holdcodes.h"
#include "exit.h"
#include <unordered_map>

int BaseJob::periodicPolicyEvalTid = TIMER_UNSET;

int BaseJob::m_checkRemoteStatusTid = TIMER_UNSET;

std::unordered_map<PROC_ID, BaseJob*> BaseJob::JobsByProcId;
std::unordered_map<std::string, BaseJob *> BaseJob::JobsByRemoteId;

void BaseJob::BaseJobReconfig()
{
	if ( periodicPolicyEvalTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( periodicPolicyEvalTid );
		periodicPolicyEvalTid = TIMER_UNSET;
	}

	Timeslice periodic_interval;
	periodic_interval.setMinInterval( param_integer( "PERIODIC_EXPR_INTERVAL", 60 ) );
	periodic_interval.setMaxInterval( param_integer( "MAX_PERIODIC_EXPR_INTERVAL", 1200 ) );
	periodic_interval.setTimeslice( param_double( "PERIODIC_EXPR_TIMESLICE", 0.01, 0, 1 ) );
	if ( periodic_interval.getMinInterval() > 0 ) {
		periodicPolicyEvalTid = daemonCore->Register_Timer( periodic_interval,
							BaseJob::EvalAllPeriodicJobExprs,
							"EvalAllPeriodicJobExprs" );
	}

	if ( m_checkRemoteStatusTid == TIMER_UNSET ) {
		m_checkRemoteStatusTid = daemonCore->Register_Timer( 5, 60,
							BaseJob::CheckAllRemoteStatus,
							"BaseJob::CheckAllRemoteStatus" );
	}
}

BaseJob::BaseJob( ClassAd *classad )
{
	calcRuntimeStats = true;

	writeUserLog = true;
	submitLogged = false;
	executeLogged = false;
	submitFailedLogged = false;
	terminateLogged = false;
	abortLogged = false;
	evictLogged = false;
	holdLogged = false;

	exitStatusKnown = false;

	m_lastRemoteStatusUpdate = 0;
	m_currentStatusUnknown = false;

	deleteFromGridmanager = false;
	deleteFromSchedd = false;

	jobAd = classad;

	procID.cluster = procID.proc = 0;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, procID.proc );

	JobsByProcId[procID] = this;

	std::string remote_id;
	jobAd->LookupString( ATTR_GRID_JOB_ID, remote_id );
	if ( !remote_id.empty() ) {
		JobsByRemoteId[remote_id] = this;
	}

	condorState = IDLE; // Just in case lookup fails
	jobAd->LookupInteger( ATTR_JOB_STATUS, condorState );

	evaluateStateTid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&BaseJob::doEvaluateState,
								"doEvaluateState", (Service*) this );;

	jobAd->EnableDirtyTracking();
	jobAd->ClearAllDirtyFlags();

	jobLeaseSentExpiredTid = TIMER_UNSET;
	jobLeaseReceivedExpiredTid = TIMER_UNSET;
	SetJobLeaseTimers();

	jobAd->LookupInteger( ATTR_LAST_REMOTE_STATUS_UPDATE, m_lastRemoteStatusUpdate );
	jobAd->LookupBool( ATTR_CURRENT_STATUS_UNKNOWN, m_currentStatusUnknown );

	int tmp_int;
	if ( jobAd->LookupInteger( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME,
							   tmp_int ) ) {
		resourceDown = true;
	} else {
		resourceDown = false;
	}
	resourceStateKnown = false;
	resourcePingPending = false;
	resourcePingComplete = false;
}

BaseJob::~BaseJob()
{
	daemonCore->Cancel_Timer( evaluateStateTid );
	if ( jobLeaseSentExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseSentExpiredTid );
	}
	if ( jobLeaseReceivedExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseReceivedExpiredTid );
	}
	JobsByProcId.erase( procID );

	std::string remote_id;
	if ( jobAd ) {
		jobAd->LookupString( ATTR_GRID_JOB_ID, remote_id );
	}
	if ( !remote_id.empty() ) {
		JobsByRemoteId.erase(remote_id);
	}

	if ( jobAd ) {
		delete jobAd;
	}
}

void BaseJob::SetEvaluateState() const
{
	daemonCore->Reset_Timer( evaluateStateTid, 0 );
}

void BaseJob::doEvaluateState( int /* timerID */ )
{
	JobHeld( "the gridmanager can't handle this job type" );
	DoneWithJob();
}

BaseResource *BaseJob::GetResource()
{
	return NULL;
}

void BaseJob::JobSubmitted( const char * /* remote_host */)
{
}

void BaseJob::JobRunning()
{
	if ( condorState != RUNNING && condorState != HELD &&
		 condorState != REMOVED ) {

		condorState = RUNNING;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr) );

		UpdateRuntimeStats();

		if ( writeUserLog && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		requestScheddUpdate( this, false );
	}
}

void BaseJob::JobIdle()
{
	if ( condorState != IDLE && condorState != HELD &&
		 condorState != REMOVED ) {

		bool write_evict = (condorState==RUNNING);

		condorState = IDLE;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr) );

		UpdateRuntimeStats();

		if( write_evict ) {
			WriteEvictEventToUserLog( jobAd );
			executeLogged = false;
		}

		requestScheddUpdate( this, false );
	}
}

void BaseJob::JobEvicted()
{
		// Does this imply a change to condorState IDLE?

	UpdateRuntimeStats();

		// If we had a lease with the remote resource, we don't now
	UpdateJobLeaseSent( 0 );

		//  should we be updating job ad values here?
	if ( writeUserLog && !evictLogged ) {
		WriteEvictEventToUserLog( jobAd );
		evictLogged = true;
	}
}

void BaseJob::JobTerminated()
{
	if ( condorState != HELD && condorState != COMPLETED &&
		 condorState != REMOVED ) {
		EvalOnExitJobExpr();
	}
}

void BaseJob::JobCompleted()
{
	if ( condorState != COMPLETED && condorState != HELD &&
		 condorState != REMOVED ) {

		condorState = COMPLETED;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr) );

		UpdateRuntimeStats();

		requestScheddUpdate( this, false );
	}
}

void BaseJob::DoneWithJob()
{
	deleteFromGridmanager = true;

	switch(condorState) {
		case COMPLETED:
		{
			// I never want to see this job again.
			jobAd->Assign( ATTR_JOB_MANAGED, MANAGED_DONE );
			if ( writeUserLog && !terminateLogged ) {
				WriteTerminateEventToUserLog( jobAd );
				EmailTerminateEvent( jobAd, exitStatusKnown );
				terminateLogged = true;
			}
			deleteFromSchedd = true;
		}
		break;

		case REMOVED:
		{
			// I never want to see this job again.
			jobAd->Assign( ATTR_JOB_MANAGED, MANAGED_DONE );
			if ( writeUserLog && !abortLogged ) {
				WriteAbortEventToUserLog( jobAd );
				abortLogged = true;
			}
			deleteFromSchedd = true;
		}
		break;

		case HELD:
		{
			jobAd->Assign( ATTR_JOB_MANAGED, MANAGED_SCHEDD );
			if ( writeUserLog && !holdLogged ) {
				WriteHoldEventToUserLog( jobAd );
				holdLogged = true;
			}
		}
		break;

		case IDLE:
		{
			jobAd->Assign( ATTR_JOB_MANAGED, MANAGED_SCHEDD );
		}
		break;

		default:
		{
			EXCEPT("BaseJob::DoneWithJob called with unexpected state %s (%d)", getJobStatusString(condorState), condorState);
		}
		break;
	}

	requestScheddUpdate( this, false );
}

void BaseJob::JobHeld( const char *hold_reason, int hold_code,
					   int hold_sub_code )
{
	bool nonessential = false;

	jobAd->LookupBool(ATTR_JOB_NONESSENTIAL,nonessential);
	if ( nonessential ) {
		// don't have the gridmanager put a job on  hold if
		// the job is nonessential.  instead, just remove it.		
		JobRemoved(hold_reason);
		return;
	}
	if ( condorState != HELD ) {
			// if the job was in REMOVED state, make certain we return
			// to the removed state when it is released.
		if ( condorState == REMOVED ) {
			jobAd->Assign( ATTR_JOB_STATUS_ON_RELEASE, REMOVED );
		}
		condorState = HELD;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr) );

		jobAd->Assign( ATTR_HOLD_REASON, hold_reason );
		jobAd->Assign(ATTR_HOLD_REASON_CODE, hold_code);
		jobAd->Assign(ATTR_HOLD_REASON_SUBCODE, hold_sub_code);

		std::string release_reason;
		if ( jobAd->LookupString( ATTR_RELEASE_REASON, release_reason ) != 0 ) {
			jobAd->Assign( ATTR_LAST_RELEASE_REASON, release_reason );
		}
		jobAd->AssignExpr( ATTR_RELEASE_REASON, "Undefined" );

		int num_holds;
		jobAd->LookupInteger( ATTR_NUM_SYSTEM_HOLDS, num_holds );
		num_holds++;
		jobAd->Assign( ATTR_NUM_SYSTEM_HOLDS, num_holds );

		UpdateRuntimeStats();

		if ( writeUserLog && !holdLogged ) {
			WriteHoldEventToUserLog( jobAd );
			holdLogged = true;
		}

		requestScheddUpdate( this, false );
	}
}

void BaseJob::JobRemoved( const char *remove_reason )
{
	if ( condorState != REMOVED ) {
		condorState = REMOVED;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr) );

		jobAd->Assign( ATTR_REMOVE_REASON, remove_reason );

		UpdateRuntimeStats();

		requestScheddUpdate( this, false );
	}
}

void BaseJob::UpdateRuntimeStats()
{
	if ( calcRuntimeStats == false ) {
		return;
	}

	// Adjust run time for condor_q
	time_t shadowBirthdate = 0;
	jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadowBirthdate );
	if ( condorState == RUNNING && shadowBirthdate == 0 ) {

		// The job has started a new interval of running
		time_t current_time = time(nullptr);
		time_t last_start_date = 0;
		jobAd->Assign( ATTR_SHADOW_BIRTHDATE, current_time );
		if ( jobAd->LookupInteger( ATTR_JOB_START_DATE, last_start_date ) == 0 ) {
			jobAd->Assign( ATTR_JOB_START_DATE, current_time );
		}
		if ( jobAd->LookupInteger( ATTR_JOB_CURRENT_START_DATE, last_start_date ) ) {
			jobAd->Assign( ATTR_JOB_LAST_START_DATE, last_start_date );
		}
		jobAd->Assign( ATTR_JOB_CURRENT_START_DATE, current_time );
		jobAd->Assign( ATTR_JOB_CURRENT_START_EXECUTING_DATE, current_time );

		int num_job_starts = 0;
		jobAd->LookupInteger( ATTR_NUM_JOB_STARTS, num_job_starts );
		num_job_starts++;
		jobAd->Assign( ATTR_NUM_JOB_STARTS, num_job_starts );
		jobAd->Assign( ATTR_JOB_RUN_COUNT, num_job_starts );

		requestScheddUpdate( this, false );

	} else if ( condorState != RUNNING && shadowBirthdate != 0 ) {

		// The job has stopped an interval of running, add the current
		// interval to the accumulated total run time
		double accum_time = 0;
		jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		accum_time += (float)( time(NULL) - shadowBirthdate );
		jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		jobAd->AssignExpr( ATTR_JOB_WALL_CLOCK_CKPT, "Undefined" );
		jobAd->AssignExpr( ATTR_SHADOW_BIRTHDATE, "UNDEFINED" );

		requestScheddUpdate( this, false );

	}
}

void BaseJob::SetRemoteJobId( const char *job_id )
{
	std::string old_job_id;
	std::string new_job_id;
	jobAd->LookupString( ATTR_GRID_JOB_ID, old_job_id );
	if ( job_id != NULL && job_id[0] != '\0' ) {
		new_job_id = job_id;
	}
	if ( old_job_id == new_job_id ) {
		return;
	}
	if ( !old_job_id.empty() ) {
		JobsByRemoteId.erase(old_job_id);
		jobAd->AssignExpr( ATTR_GRID_JOB_ID, "Undefined" );
	} else {
		//  old job id was NULL
		m_lastRemoteStatusUpdate = time(NULL);
		jobAd->Assign( ATTR_LAST_REMOTE_STATUS_UPDATE, m_lastRemoteStatusUpdate );
	}
	if ( !new_job_id.empty() ) {
		JobsByRemoteId[new_job_id] = this;
		jobAd->Assign( ATTR_GRID_JOB_ID, new_job_id );
	} else {
		// new job id is NULL
		m_lastRemoteStatusUpdate = 0;
		jobAd->Assign( ATTR_LAST_REMOTE_STATUS_UPDATE, 0 );
		m_currentStatusUnknown = false;
		jobAd->Assign( ATTR_CURRENT_STATUS_UNKNOWN, false );
	}
	requestScheddUpdate( this, false );
}

bool BaseJob::SetRemoteJobStatus( const char *job_status )
{
	std::string old_job_status;
	std::string new_job_status;

	if ( job_status ) {
		m_lastRemoteStatusUpdate = time(NULL);
		jobAd->Assign( ATTR_LAST_REMOTE_STATUS_UPDATE, m_lastRemoteStatusUpdate );
		requestScheddUpdate( this, false );
		if ( m_currentStatusUnknown == true ) {
			m_currentStatusUnknown = false;
			jobAd->Assign( ATTR_CURRENT_STATUS_UNKNOWN, false );
			WriteJobStatusKnownEventToUserLog( jobAd );
		}
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, old_job_status );
	if ( job_status != NULL && job_status[0] != '\0' ) {
		new_job_status = job_status;
	}
	if ( old_job_status == new_job_status ) {
		return false;
	}
	if ( !old_job_status.empty() ) {
		jobAd->AssignExpr( ATTR_GRID_JOB_STATUS, "Undefined" );
	}
	if ( !new_job_status.empty() ) {
		jobAd->Assign( ATTR_GRID_JOB_STATUS, new_job_status );
	}
	requestScheddUpdate( this, false );
	return true;
}

void BaseJob::SetJobLeaseTimers()
{
dprintf(D_FULLDEBUG,"(%d.%d) SetJobLeaseTimers()\n",procID.cluster,procID.proc);
	int expiration_time = -1;

	jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, expiration_time );

	if ( expiration_time == -1 ) {
		if ( jobLeaseSentExpiredTid != TIMER_UNSET ) {
			daemonCore->Cancel_Timer( jobLeaseSentExpiredTid );
			jobLeaseSentExpiredTid = TIMER_UNSET;
		}
	} else {
		int when = expiration_time - time(NULL);
		if ( when < 0 ) {
			when = 0;
		}
		if ( jobLeaseSentExpiredTid == TIMER_UNSET ) {
			jobLeaseSentExpiredTid = daemonCore->Register_Timer(
								when,
								(TimerHandlercpp)&BaseJob::JobLeaseSentExpired,
								"JobLeaseSentExpired", (Service*) this );
		} else {
			daemonCore->Reset_Timer( jobLeaseSentExpiredTid, when );
		}
	}

	expiration_time = -1;

	jobAd->LookupInteger( ATTR_TIMER_REMOVE_CHECK, expiration_time );

	if ( expiration_time == -1 ) {
		if ( jobLeaseReceivedExpiredTid != TIMER_UNSET ) {
			daemonCore->Cancel_Timer( jobLeaseReceivedExpiredTid );
			jobLeaseReceivedExpiredTid = TIMER_UNSET;
		}
	} else {
		int when = expiration_time - time(NULL);
		if ( when < 0 ) {
			when = 0;
		}
		if ( jobLeaseReceivedExpiredTid == TIMER_UNSET ) {
			jobLeaseReceivedExpiredTid = daemonCore->Register_Timer(
							when,
							(TimerHandlercpp)&BaseJob::JobLeaseReceivedExpired,
							"JobLeaseReceivedExpired", (Service*) this );
		} else {
			daemonCore->Reset_Timer( jobLeaseReceivedExpiredTid, when );
		}
	}
}

void BaseJob::UpdateJobLeaseSent( time_t new_expiration_time )
{
dprintf(D_FULLDEBUG,"(%d.%d) UpdateJobLeaseSent(%lld)\n",procID.cluster,procID.proc,(long long)new_expiration_time);
	time_t old_expiration_time = TIMER_UNSET;

	jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION,
						  old_expiration_time );

	if ( new_expiration_time <= 0 ) {
		new_expiration_time = TIMER_UNSET;
	}

	if ( new_expiration_time != old_expiration_time ) {

		if ( new_expiration_time == TIMER_UNSET ) {
			jobAd->AssignExpr( ATTR_JOB_LEASE_EXPIRATION, "Undefined" );
		} else {
			jobAd->Assign( ATTR_JOB_LEASE_EXPIRATION,
						   new_expiration_time );
		}

		if ( old_expiration_time == TIMER_UNSET ||
			 ( new_expiration_time != TIMER_UNSET &&
			   new_expiration_time < old_expiration_time ) ) {

			BaseResource *resource = GetResource();
			if ( resource ) {
				resource->RequestUpdateLeases();
			}
		}

		requestScheddUpdate( this, false );

		SetJobLeaseTimers();
	}
}

void BaseJob::UpdateJobLeaseReceived( time_t new_expiration_time )
{
	time_t old_expiration_time = TIMER_UNSET;
dprintf(D_FULLDEBUG,"(%d.%d) UpdateJobLeaseReceived(%lld)\n",procID.cluster,procID.proc,(long long)new_expiration_time);

	jobAd->LookupInteger( ATTR_TIMER_REMOVE_CHECK, old_expiration_time );

	if ( new_expiration_time <= 0 ) {
		new_expiration_time = TIMER_UNSET;
	}

	if ( new_expiration_time != old_expiration_time ) {

		if ( old_expiration_time == TIMER_UNSET ) {
			dprintf( D_ALWAYS, "(%d.%d) New lease but no old lease, ignoring!\n",
					 procID.cluster, procID.proc );
			return;
		}

		if ( new_expiration_time < old_expiration_time ) {
			dprintf( D_ALWAYS, "(%d.%d) New lease expiration (%lld) is older than old lease expiration (%lld), ignoring!\n",
			         procID.cluster, procID.proc, (long long)new_expiration_time,
			         (long long)old_expiration_time );
			return;
		}

		jobAd->Assign( ATTR_TIMER_REMOVE_CHECK, new_expiration_time );
		jobAd->MarkAttributeClean( ATTR_TIMER_REMOVE_CHECK );

		SetJobLeaseTimers();
	}
}

void BaseJob::JobLeaseSentExpired( int /* timerID */ )
{
dprintf(D_FULLDEBUG,"(%d.%d) BaseJob::JobLeaseSentExpired()\n",procID.cluster,procID.proc);
	if ( jobLeaseSentExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseSentExpiredTid );
		jobLeaseSentExpiredTid = TIMER_UNSET;
	}
	SetEvaluateState();
}

void BaseJob::JobLeaseReceivedExpired( int /* timerID */ )
{
dprintf(D_FULLDEBUG,"(%d.%d) BaseJob::JobLeaseReceivedExpired()\n",procID.cluster,procID.proc);
	if ( jobLeaseReceivedExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseReceivedExpiredTid );
		jobLeaseReceivedExpiredTid = TIMER_UNSET;
	}

	condorState = REMOVED;
	jobAd->Assign( ATTR_JOB_STATUS, condorState );
	jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, time(nullptr));

	jobAd->Assign( ATTR_REMOVE_REASON, "Job lease expired" );

	UpdateRuntimeStats();

	requestScheddUpdate( this, false );

	SetEvaluateState();
}

void BaseJob::JobAdUpdateFromSchedd( const ClassAd *new_ad, bool full_ad )
{
	static const char *held_removed_update_attrs[] = {
		ATTR_JOB_STATUS,
		ATTR_HOLD_REASON,
		ATTR_HOLD_REASON_CODE,
		ATTR_HOLD_REASON_SUBCODE,
		ATTR_LAST_HOLD_REASON,
		ATTR_RELEASE_REASON,
		ATTR_LAST_RELEASE_REASON,
		ATTR_ENTERED_CURRENT_STATUS,
		ATTR_NUM_SYSTEM_HOLDS,
		ATTR_REMOVE_REASON,
		NULL
	};

	int new_condor_state;

	new_ad->LookupInteger( ATTR_JOB_STATUS, new_condor_state );

	if ( new_condor_state == condorState ) {
		if ( !full_ad ) {
			MergeClassAds( jobAd, new_ad, true, false );
		}
		return;
	}

	if ( new_condor_state == REMOVED && condorState == HELD ) {
		int release_status = IDLE;
		jobAd->LookupInteger( ATTR_JOB_STATUS_ON_RELEASE, release_status );
		if ( release_status == REMOVED ) {
				// We already know about this REMOVED state and have
				// decided to go on hold afterwards, so ignore this
				// "update".
			return;
		}
	}

	if ( new_condor_state == REMOVED || new_condor_state == HELD ) {

		for ( int i = 0; held_removed_update_attrs[i] != NULL; i++ ) {
			ExprTree *expr;

			if ( (expr = new_ad->LookupExpr( held_removed_update_attrs[i] )) != NULL ) {
				ExprTree * pTree = expr->Copy();
				jobAd->Insert( held_removed_update_attrs[i], pTree );
			} else {
				jobAd->Delete( held_removed_update_attrs[i] );
			}
			jobAd->MarkAttributeClean( held_removed_update_attrs[i] );
		}

		if ( new_condor_state == HELD && writeUserLog && !holdLogged ) {
			// TODO should this log event be delayed until gridmanager is
			//   done dealing with the job?
			WriteHoldEventToUserLog( jobAd );
			holdLogged = true;
		}

			// If we're about to put a job on hold and learn that it's been
			// removed, make sure the state returns to removed when it is
			// released. This is normally checked in JobHeld(), but it's
			// possible to learn of the removal just as we're about to
			// update the schedd with the hold.
		if ( new_condor_state == REMOVED && condorState == HELD ) {
			if ( jobAd->IsAttributeDirty( ATTR_JOB_STATUS ) ) {
				jobAd->Assign( ATTR_JOB_STATUS_ON_RELEASE, REMOVED );
			}
		}

		condorState = new_condor_state;
		// TODO do we need to call UpdateRuntimeStats() here?
		UpdateRuntimeStats();
		SetEvaluateState();

	} else if ( new_condor_state == COMPLETED ) {

		condorState = new_condor_state;
			// TODO do we need to update any other attributes?
		SetEvaluateState();
	} else if ( !full_ad ) {
		MergeClassAds( jobAd, new_ad, true, false );
	}

}

void BaseJob::EvalAllPeriodicJobExprs(int /* tid */)
{
	dprintf( D_FULLDEBUG, "Evaluating periodic job policy expressions.\n" );

	for (auto& itr: JobsByProcId) {
		itr.second->EvalPeriodicJobExpr();
	}
}

int BaseJob::EvalPeriodicJobExpr()
{
	float old_run_time;
	bool old_run_time_dirty;
	UserPolicy user_policy;

	user_policy.Init();

	UpdateJobTime( &old_run_time, &old_run_time_dirty );

	int action = user_policy.AnalyzePolicy( *jobAd, PERIODIC_ONLY, condorState );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	std::string reason_buf;
	int reason_code;
	int reason_subcode;
	user_policy.FiringReason(reason_buf,reason_code,reason_subcode);
	char const *reason = reason_buf.c_str();
	if ( reason == NULL || !reason[0] ) {
		reason = "Unknown user policy expression";
	}

	switch( action ) {
	case UNDEFINED_EVAL:
	case HOLD_IN_QUEUE:
		JobHeld( reason, reason_code, reason_subcode );
		SetEvaluateState();
		break;
	case STAYS_IN_QUEUE:
			// do nothing
		break;
	case REMOVE_FROM_QUEUE:
		JobRemoved( reason );
		SetEvaluateState();
		break;
	case RELEASE_FROM_HOLD:
			// When a job gets held and then released while the gridmanager
			// is managing it, the gridmanager cleans up and deletes its
			// local data for the job (canceling the remote submission if
			// possible), then picks it up as a new job from the schedd.
			// So ignore release-from-hold and let the schedd deal with it.
		break;
	case VACATE_FROM_RUNNING:
			// ignore this case, as we can't really handle it in the general
			// case
		break;
	default:
		EXCEPT( "Unknown action (%d) in BaseJob::EvalPeriodicJobExpr", 
				action );
	}

	return 0;
}

int BaseJob::EvalOnExitJobExpr()
{
	float old_run_time;
	bool old_run_time_dirty;
	UserPolicy user_policy;

	user_policy.Init();

	// The user policy code expects an exit value to be set
	// If the ON_EXIT attributes haven't been set at all, fake
	// a normal job exit.
	int dummy;
	if ( !jobAd->LookupInteger( ATTR_ON_EXIT_SIGNAL, dummy ) &&
		 !jobAd->LookupInteger( ATTR_ON_EXIT_CODE, dummy ) ) {

		exitStatusKnown = false;
		jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
		jobAd->Assign( ATTR_ON_EXIT_CODE, 0 );
	} else {
		exitStatusKnown = true;
	}

	// TODO: We should just mark the job as done running
	UpdateJobTime( &old_run_time, &old_run_time_dirty );

	int action = user_policy.AnalyzePolicy( *jobAd, PERIODIC_THEN_EXIT, condorState );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	if ( action != REMOVE_FROM_QUEUE ) {
		jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
		jobAd->AssignExpr( ATTR_ON_EXIT_CODE, "Undefined" );
		jobAd->AssignExpr( ATTR_ON_EXIT_SIGNAL, "Undefined" );
	}

	std::string reason_buf;
	int reason_code;
	int reason_subcode;
	user_policy.FiringReason(reason_buf,reason_code,reason_subcode);
	const char *reason = reason_buf.c_str();
	if ( reason == NULL || !reason[0] ) {
		reason = "Unknown user policy expression";
	}

	switch( action ) {
	case UNDEFINED_EVAL:
	case HOLD_IN_QUEUE:
		JobHeld( reason, reason_code, reason_subcode );
		break;
	case STAYS_IN_QUEUE:
			// clean up job but don't set status to complete
		break;
	case REMOVE_FROM_QUEUE:
		JobCompleted();
		break;
	case VACATE_FROM_RUNNING:
			// ignore this case, as we can't really handle it in the general
			// case
		break;
	default:
		EXCEPT( "Unknown action (%d) in BaseJob::EvalAtExitJobExpr", 
				action );
	}

	return 0;
}

void BaseJob::CheckAllRemoteStatus(int /* tid */)
{
	dprintf( D_FULLDEBUG, "Evaluating staleness of remote job statuses.\n" );

		// TODO Reset timer based on shortest time that a job status could
		//   become stale?
	for (auto& iter: JobsByProcId) {
		iter.second->CheckRemoteStatus();
	}
}

void BaseJob::CheckRemoteStatus()
{
	const int stale_limit = 15*60;

		// TODO return time that this job status could become stale?
		// TODO compute stale_limit from job's poll interval?
		// TODO make stale_limit configurable?
	if ( m_lastRemoteStatusUpdate == 0 || m_currentStatusUnknown == true ) {
		return;
	}
	if ( time(NULL) > m_lastRemoteStatusUpdate + stale_limit ) {
		m_currentStatusUnknown = true;
		jobAd->Assign( ATTR_CURRENT_STATUS_UNKNOWN, true );
		requestScheddUpdate( this, false );
		WriteJobStatusUnknownEventToUserLog( jobAd );
		SetEvaluateState();
	}
}

/*Before evaluating user policy expressions, temporarily update
  any stale time values.  Currently, this is just RemoteWallClock.
*/
void
BaseJob::UpdateJobTime( float *old_run_time, bool *old_run_time_dirty ) const
{
  double previous_run_time = 0, total_run_time = 0;
  time_t shadow_bday = 0;
  time_t now = time(NULL);

  jobAd->LookupInteger(ATTR_SHADOW_BIRTHDATE,shadow_bday);
  jobAd->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,previous_run_time);
  *old_run_time_dirty = jobAd->IsAttributeDirty(ATTR_JOB_REMOTE_WALL_CLOCK);

  if (old_run_time) {
	  *old_run_time = previous_run_time;
  }
  total_run_time = previous_run_time;
  if (shadow_bday) {
	  total_run_time += (now - shadow_bday);
  }

  jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, total_run_time );
}

/*After evaluating user policy expressions, this is
  called to restore time values to their original state.
*/
void
BaseJob::RestoreJobTime( float old_run_time, bool old_run_time_dirty ) const
{
	jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time );
	if ( old_run_time_dirty ) {
		jobAd->MarkAttributeDirty( ATTR_JOB_REMOTE_WALL_CLOCK );
	} else {
		jobAd->MarkAttributeClean( ATTR_JOB_REMOTE_WALL_CLOCK );
	}
}

void BaseJob::RequestPing()
{
	BaseResource *resource = this->GetResource();
	if ( resource != NULL ) {
		resourcePingPending = true;
		resourcePingComplete = false;
		resource->RequestPing( this );
	} else {
		dprintf( D_ALWAYS,
				 "(%d.%d) RequestPing(): GetResource returned NULL\n",
				 procID.cluster, procID.proc );
	}
}

void BaseJob::NotifyResourceDown()
{
	resourceStateKnown = true;
	if ( resourceDown == false ) {
		WriteGridResourceDownEventToUserLog( jobAd );
		jobAd->Assign( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME, time(nullptr) );
		requestScheddUpdate( this, false );
	}
	resourceDown = true;
	if ( resourcePingPending ) {
		resourcePingPending = false;
		resourcePingComplete = true;
	}
	SetEvaluateState();
}

void BaseJob::NotifyResourceUp()
{
	resourceStateKnown = true;
	if ( resourceDown == true ) {
		WriteGridResourceUpEventToUserLog( jobAd );
		jobAd->AssignExpr( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME, "Undefined" );
		requestScheddUpdate( this, false );
	}
	resourceDown = false;
	if ( resourcePingPending ) {
		resourcePingPending = false;
		resourcePingComplete = true;
	}
	SetEvaluateState();
}

bool
WriteExecuteEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char hostname[128];

	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing execute record to user logfile\n",
			 cluster, proc );

	hostname[0] = '\0';
	job_ad->LookupString( ATTR_GRID_RESOURCE, hostname,
						  sizeof(hostname) );

	ExecuteEvent event;
	event.setExecuteHost( hostname );
	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_EXECUTE event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteAbortEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	JobAbortedEvent event;

	std::string reasonstr;
	job_ad->LookupString(ATTR_REMOVE_REASON, reasonstr);
	event.setReason(reasonstr);

	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_ABORT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteTerminateEventToUserLog( ClassAd *job_ad )
{
	if ( ! job_ad ) {
		dprintf( D_ALWAYS,
				 "Internal Error: WriteTerminateEventToUserLog passed null "
				 "ClassAd.\n" );
		return false;
	}

	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing terminate record to user logfile\n",
			 cluster, proc );

	JobTerminatedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
	event.total_local_rusage = r;
	event.total_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;
	event.total_sent_bytes = 0;
	event.total_recvd_bytes = 0;

	int int_val;
	bool bool_val;
	if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, bool_val) ) {
		if( bool_val ) {
			if( job_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
				event.signalNumber = int_val;
				event.normal = false;
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				event.normal = false;
					// TODO What about event.signalNumber?
			}
		} else {
			if( job_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
				event.normal = true;
				event.returnValue = int_val;
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				event.normal = false;
					// TODO What about event.signalNumber?
			}
		}
	} else {
		dprintf( D_ALWAYS,
				 "(%d.%d) Job ad lacks %s.  Final state unknown.\n",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		event.normal = false;
			// TODO What about event.signalNumber?
	}

	double real_val = 0;
	if ( job_ad->LookupFloat( ATTR_JOB_REMOTE_USER_CPU, real_val ) ) {
		event.run_remote_rusage.ru_utime.tv_sec = (time_t)real_val;
		event.total_remote_rusage.ru_utime.tv_sec = (time_t)real_val;
	}
	if ( job_ad->LookupFloat( ATTR_JOB_REMOTE_SYS_CPU, real_val ) ) {
		event.run_remote_rusage.ru_stime.tv_sec = (time_t)real_val;
		event.total_remote_rusage.ru_stime.tv_sec = (time_t)real_val;
	}

	setEventUsageAd(*job_ad, &event.pusageAd);

	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_TERMINATED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteEvictEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing evict record to user logfile\n",
			 cluster, proc );

	JobEvictedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;

	event.checkpointed = false;

	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_EVICTED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteHoldEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;

	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing hold record to user logfile\n",
			 cluster, proc );

	JobHeldEvent event;

	event.initFromClassAd(job_ad);

	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_HELD event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGridResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid resource up record to user logfile\n",
			 cluster, proc );

	GridResourceUpEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, event.resourceName );
	if ( event.resourceName.empty() ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) %s attribute missing in job ad\n",
				 cluster, proc, ATTR_GRID_RESOURCE );
	}

	int rc = ulog.writeEvent( &event, job_ad );

	if ( !rc ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GRID_RESOURCE_UP event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGridResourceDownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid source down record to user logfile\n",
			 cluster, proc );

	GridResourceDownEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, event.resourceName );
	if ( event.resourceName.empty() ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) %s attribute missing in job ad\n",
				 cluster, proc, ATTR_GRID_RESOURCE );
	}

	int rc = ulog.writeEvent(&event,job_ad);

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GRID_RESOURCE_DOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGridSubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid submit record to user logfile\n",
			 cluster, proc );

	GridSubmitEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, event.resourceName );

	job_ad->LookupString( ATTR_GRID_JOB_ID, event.jobId );

	int rc = ulog.writeEvent( &event,job_ad );

	if ( !rc ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GRID_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteJobStatusUnknownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing job status unknown record to user logfile\n",
			 cluster, proc );

	JobStatusUnknownEvent event;

	int rc = ulog.writeEvent( &event, job_ad );

	if ( !rc ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_STATUS_UNKNOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteJobStatusKnownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	WriteUserLog ulog;
	// TODO Check return value of initialize()
	ulog.initialize( *job_ad );
	if ( ! ulog.willWrite() ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing job status known record to user logfile\n",
			 cluster, proc );

	JobStatusKnownEvent event;

	int rc = ulog.writeEvent( &event, job_ad );

	if ( !rc ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_STATUS_KNOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

void
EmailTerminateEvent(ClassAd * job_ad, bool   /*exit_status_known*/)
{
	if ( !job_ad ) {
		dprintf(D_ALWAYS, 
				"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	Email email;
	email.sendExit(job_ad, JOB_EXITED);
}
