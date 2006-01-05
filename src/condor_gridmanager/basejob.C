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


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_ckpt_name.h"
#include "globus_utils.h" // for GRAM_V_1_5

#include "gridmanager.h"
#include "basejob.h"
#include "condor_config.h"
#include "condor_email.h"

#define HASH_TABLE_SIZE			500


int BaseJob::periodicPolicyEvalTid = TIMER_UNSET;

HashTable<PROC_ID, BaseJob *> BaseJob::JobsByProcId( HASH_TABLE_SIZE,
													 hashFuncPROC_ID );
HashTable<HashKey, BaseJob *> BaseJob::JobsByRemoteId( HASH_TABLE_SIZE,
													   hashFunction );

void BaseJob::BaseJobReconfig()
{
	int tmp_int;

	if ( periodicPolicyEvalTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( periodicPolicyEvalTid );
		periodicPolicyEvalTid = TIMER_UNSET;
	}

	tmp_int = param_integer( "PERIODIC_EXPR_INTERVAL", 300 );
	if ( tmp_int != 0 ) {
		periodicPolicyEvalTid = daemonCore->Register_Timer( tmp_int, tmp_int,
							(TimerHandler)&BaseJob::EvalAllPeriodicJobExprs,
							"EvalAllPeriodicJobExprs", (Service*)NULL );
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

	deleteFromGridmanager = false;
	deleteFromSchedd = false;

	jobAd = classad;

	jobAd->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, procID.proc );

	JobsByProcId.insert( procID, this );

	MyString remote_id;
	jobAd->LookupString( ATTR_GRID_JOB_ID, remote_id );
	if ( !remote_id.IsEmpty() ) {
		JobsByRemoteId.insert( HashKey( remote_id.Value() ), this );
	}

	jobAd->LookupInteger( ATTR_JOB_STATUS, condorState );

	evaluateStateTid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&BaseJob::doEvaluateState,
								"doEvaluateState", (Service*) this );;

	wantRematch = 0;
	doResubmit = 0;		// set if gridmanager wants to resubmit job
	wantResubmit = 0;	// set if user wants to resubmit job via RESUBMIT_CHECK
	jobAd->EvalBool(ATTR_GLOBUS_RESUBMIT_CHECK,NULL,wantResubmit);

	jobAd->ClearAllDirtyFlags();

	jobLeaseSentExpiredTid = TIMER_UNSET;
	jobLeaseReceivedExpiredTid = TIMER_UNSET;
	SetJobLeaseTimers();

	resourceStateKnown = false;
	resourceDown = false;
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
	JobsByProcId.remove( procID );

	MyString remote_id;
	jobAd->LookupString( ATTR_GRID_JOB_ID, remote_id );
	if ( !remote_id.IsEmpty() ) {
		JobsByRemoteId.remove( HashKey( remote_id.Value() ) );
	}

	if ( jobAd ) {
		delete jobAd;
	}
}

void BaseJob::SetEvaluateState()
{
	daemonCore->Reset_Timer( evaluateStateTid, 0 );
}

int BaseJob::doEvaluateState()
{
	JobHeld( "the gridmanager can't handle this job type" );
	DoneWithJob();
	return TRUE;
}

BaseResource *BaseJob::GetResource()
{
	return NULL;
}

void BaseJob::JobSubmitted( const char *remote_host)
{
}

void BaseJob::JobRunning()
{
	if ( condorState != RUNNING && condorState != HELD &&
		 condorState != REMOVED ) {

		condorState = RUNNING;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

		UpdateRuntimeStats();

		if ( writeUserLog && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		requestScheddUpdate( this );
	}
}

void BaseJob::JobIdle()
{
	if ( condorState != IDLE && condorState != HELD &&
		 condorState != REMOVED ) {

		condorState = IDLE;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

		UpdateRuntimeStats();

		requestScheddUpdate( this );
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
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

		UpdateRuntimeStats();

		requestScheddUpdate( this );
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
			EXCEPT("BaseJob::DoneWithJob called with unexpected state %s (%d)\n", getJobStatusString(condorState), condorState);
		}
		break;
	}

	requestScheddUpdate( this );
}

void BaseJob::JobHeld( const char *hold_reason, int hold_code,
					   int hold_sub_code )
{
	if ( condorState != HELD ) {
			// if the job was in REMOVED state, make certain we return
			// to the removed state when it is released.
		if ( condorState == REMOVED ) {
			jobAd->Assign( ATTR_JOB_STATUS_ON_RELEASE, REMOVED );
		}
		condorState = HELD;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

		jobAd->Assign( ATTR_HOLD_REASON, hold_reason );
		jobAd->Assign(ATTR_HOLD_REASON_CODE, hold_code);
		jobAd->Assign(ATTR_HOLD_REASON_SUBCODE, hold_sub_code);

		char *release_reason;
		if ( jobAd->LookupString( ATTR_RELEASE_REASON, &release_reason ) != 0 ) {
			jobAd->Assign( ATTR_LAST_RELEASE_REASON, release_reason );
			free( release_reason );
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

		requestScheddUpdate( this );
	}
}

void BaseJob::JobRemoved( const char *remove_reason )
{
	if ( condorState != REMOVED ) {
		condorState = REMOVED;
		jobAd->Assign( ATTR_JOB_STATUS, condorState );
		jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

		jobAd->Assign( ATTR_REMOVE_REASON, remove_reason );

		UpdateRuntimeStats();

		requestScheddUpdate( this );
	}
}

void BaseJob::UpdateRuntimeStats()
{
	if ( calcRuntimeStats == false ) {
		return;
	}

	// Adjust run time for condor_q
	int shadowBirthdate = 0;
	jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadowBirthdate );
	if ( condorState == RUNNING && shadowBirthdate == 0 ) {

		// The job has started a new interval of running
		int current_time = (int)time(NULL);
		jobAd->Assign( ATTR_SHADOW_BIRTHDATE, current_time );

		requestScheddUpdate( this );

	} else if ( condorState != RUNNING && shadowBirthdate != 0 ) {

		// The job has stopped an interval of running, add the current
		// interval to the accumulated total run time
		float accum_time = 0;
		jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		accum_time += (float)( time(NULL) - shadowBirthdate );
		jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		jobAd->Assign( ATTR_JOB_WALL_CLOCK_CKPT,(char *)NULL );
		jobAd->Assign( ATTR_SHADOW_BIRTHDATE, 0 );

		requestScheddUpdate( this );

	}
}

void BaseJob::SetRemoteJobId( const char *job_id )
{
	MyString old_job_id;
	MyString new_job_id;
	jobAd->LookupString( ATTR_GRID_JOB_ID, old_job_id );
	if ( job_id != NULL && job_id[0] != '\0' ) {
		new_job_id = job_id;
	}
	if ( old_job_id == new_job_id ) {
		return;
	}
	if ( !old_job_id.IsEmpty() ) {
		JobsByRemoteId.remove( HashKey( old_job_id.Value() ) );
		jobAd->AssignExpr( ATTR_GRID_JOB_ID, "Undefined" );
	}
	if ( !new_job_id.IsEmpty() ) {
		JobsByRemoteId.insert( HashKey( new_job_id.Value() ), this );
		jobAd->Assign( ATTR_GRID_JOB_ID, new_job_id.Value() );
	}
	requestScheddUpdate( this );
}

void BaseJob::SetJobLeaseTimers()
{
dprintf(D_FULLDEBUG,"(%d.%d) SetJobLeaseTimers()\n",procID.cluster,procID.proc);
	int expiration_time = -1;

	jobAd->LookupInteger( ATTR_TIMER_REMOVE_CHECK_SENT, expiration_time );

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

void BaseJob::UpdateJobLeaseSent( int new_expiration_time )
{
dprintf(D_FULLDEBUG,"(%d.%d) UpdateJobLeaseSent(%d)\n",procID.cluster,procID.proc,(int)new_expiration_time);
	int old_expiration_time = TIMER_UNSET;

	jobAd->LookupInteger( ATTR_TIMER_REMOVE_CHECK_SENT,
						  old_expiration_time );

	if ( new_expiration_time <= 0 ) {
		new_expiration_time = TIMER_UNSET;
	}

	if ( new_expiration_time != old_expiration_time ) {

		if ( new_expiration_time == TIMER_UNSET ) {
			jobAd->AssignExpr( ATTR_TIMER_REMOVE_CHECK_SENT, "Undefined" );
			jobAd->AssignExpr( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
							   "Undefined" );
		} else {
			jobAd->Assign( ATTR_TIMER_REMOVE_CHECK_SENT,
						   new_expiration_time );
		}

		requestScheddUpdate( this );

		SetJobLeaseTimers();
	}
}

void BaseJob::UpdateJobLeaseReceived( int new_expiration_time )
{
	int old_expiration_time = TIMER_UNSET;
dprintf(D_FULLDEBUG,"(%d.%d) UpdateJobLeaseReceived(%d)\n",procID.cluster,procID.proc,(int)new_expiration_time);

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
			dprintf( D_ALWAYS, "(%d.%d) New lease expiration (%d) is older than old lease expiration (%d), ignoring!\n",
					 procID.cluster, procID.proc, new_expiration_time,
					 old_expiration_time );
			return;
		}

		jobAd->Assign( ATTR_TIMER_REMOVE_CHECK, new_expiration_time );
		jobAd->SetDirtyFlag( ATTR_TIMER_REMOVE_CHECK, false );

		SetJobLeaseTimers();
	}
}

int BaseJob::JobLeaseSentExpired()
{
dprintf(D_FULLDEBUG,"(%d.%d) BaseJob::JobLeaseSentExpired()\n",procID.cluster,procID.proc);
	if ( jobLeaseSentExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseSentExpiredTid );
		jobLeaseSentExpiredTid = TIMER_UNSET;
	}
	SetEvaluateState();
	return 0;
}

int BaseJob::JobLeaseReceivedExpired()
{
dprintf(D_FULLDEBUG,"(%d.%d) BaseJob::JobLeaseReceivedExpired()\n",procID.cluster,procID.proc);
	if ( jobLeaseReceivedExpiredTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( jobLeaseReceivedExpiredTid );
		jobLeaseReceivedExpiredTid = TIMER_UNSET;
	}

	condorState = REMOVED;
	jobAd->Assign( ATTR_JOB_STATUS, condorState );
	jobAd->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

	jobAd->Assign( ATTR_REMOVE_REASON, "Job lease expired" );

	UpdateRuntimeStats();

	requestScheddUpdate( this );

	SetEvaluateState();
	return 0;
}

void BaseJob::JobAdUpdateFromSchedd( const ClassAd *new_ad )
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
			// The job state in the sched hasn't changed, so we can ignore
			// this "update".
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
			char attr_value[1024];
			ExprTree *expr;

			if ( (expr = new_ad->Lookup( held_removed_update_attrs[i] )) != NULL ) {
				attr_value[0] = '\0';
				expr->RArg()->PrintToStr(attr_value);
				jobAd->AssignExpr( held_removed_update_attrs[i], attr_value );
			} else {
				jobAd->Delete( held_removed_update_attrs[i] );
			}
			jobAd->SetDirtyFlag( held_removed_update_attrs[i], false );
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
			bool dirty;
			jobAd->GetDirtyFlag( ATTR_JOB_STATUS, NULL, &dirty );
			if ( dirty ) {
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
	}

}

int BaseJob::EvalAllPeriodicJobExprs(Service *ignore)
{
	BaseJob *curr_job;

	dprintf( D_FULLDEBUG, "Evaluating periodic job policy expressions.\n" );

	JobsByProcId.startIterations();
	while ( JobsByProcId.iterate( curr_job ) != 0  ) {
		curr_job->EvalPeriodicJobExpr();
	}

	return 0;
}

int BaseJob::EvalPeriodicJobExpr()
{
	float old_run_time;
	bool old_run_time_dirty;
	UserPolicy user_policy;

	user_policy.Init( jobAd );

	UpdateJobTime( &old_run_time, &old_run_time_dirty );

	int action = user_policy.AnalyzePolicy( PERIODIC_ONLY );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	MyString reason = user_policy.FiringReason();
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch( action ) {
	case UNDEFINED_EVAL:
		JobHeld( reason.Value() );
		SetEvaluateState();
		break;
	case STAYS_IN_QUEUE:
			// do nothing
		break;
	case REMOVE_FROM_QUEUE:
		JobRemoved( reason.Value() );
		SetEvaluateState();
		break;
	case HOLD_IN_QUEUE:
		JobHeld( reason.Value() );
		SetEvaluateState();
		break;
	case RELEASE_FROM_HOLD:
			// When a job gets held and then released while the gridmanager
			// is managing it, the gridmanager cleans up and deletes its
			// local data for the job (canceling the remote submission if
			// possible), then picks it up as a new job from the schedd.
			// So ignore release-from-hold and let the schedd deal with it.
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

	user_policy.Init( jobAd );

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

	int action = user_policy.AnalyzePolicy( PERIODIC_THEN_EXIT );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	if ( action != REMOVE_FROM_QUEUE ) {
		jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
		jobAd->AssignExpr( ATTR_ON_EXIT_CODE, "Undefined" );
		jobAd->AssignExpr( ATTR_ON_EXIT_SIGNAL, "Undefined" );
	}

	MyString reason = user_policy.FiringReason();
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch( action ) {
	case UNDEFINED_EVAL:
		JobHeld( reason.Value() );
		break;
	case STAYS_IN_QUEUE:
			// clean up job but don't set status to complete
		break;
	case REMOVE_FROM_QUEUE:
		JobCompleted();
		break;
	case HOLD_IN_QUEUE:
		JobHeld( reason.Value() );
		break;
	default:
		EXCEPT( "Unknown action (%d) in BaseJob::EvalAtExitJobExpr", 
				action );
	}

	return 0;
}

/*Before evaluating user policy expressions, temporarily update
  any stale time values.  Currently, this is just RemoteWallClock.
*/
void
BaseJob::UpdateJobTime( float *old_run_time, bool *old_run_time_dirty )
{
  float previous_run_time = 0, total_run_time = 0;
  int shadow_bday = 0;
  time_t now = time(NULL);

  jobAd->LookupInteger(ATTR_SHADOW_BIRTHDATE,shadow_bday);
  jobAd->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,previous_run_time);
  jobAd->GetDirtyFlag(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,old_run_time_dirty);

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
BaseJob::RestoreJobTime( float old_run_time, bool old_run_time_dirty )
{
  jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time );
  jobAd->SetDirtyFlag( ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time_dirty );
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
			// The GlobusResourceDown event is now deprecated
		WriteGlobusResourceDownEventToUserLog( jobAd );
		WriteGridResourceDownEventToUserLog( jobAd );
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
			// The GlobusResourceUp event is now deprecated
		WriteGlobusResourceUpEventToUserLog( jobAd );
		WriteGridResourceUpEventToUserLog( jobAd );
	}
	resourceDown = false;
	if ( resourcePingPending ) {
		resourcePingPending = false;
		resourcePingComplete = true;
	}
	SetEvaluateState();
}

// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
InitializeUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char userLogFile[_POSIX_PATH_MAX];
	char domain[_POSIX_PATH_MAX];


	userLogFile[0] = '\0';
	job_ad->LookupString( ATTR_ULOG_FILE, userLogFile );
	if ( userLogFile[0] == '\0' ) {
		// User doesn't want a log
		return NULL;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );
	job_ad->LookupString( ATTR_NT_DOMAIN, domain );

	UserLog *ULog = new UserLog();
	ULog->initialize(Owner, domain, userLogFile, cluster, proc, 0);
	return ULog;
}

bool
WriteExecuteEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char hostname[128];

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
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
						  sizeof(hostname) - 1 );

	ExecuteEvent event;
	strcpy( event.executeHost, hostname );
	int rc = ulog->writeEvent(&event);
	delete ulog;

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
	char removeReason[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	JobAbortedEvent event;

	removeReason[0] = '\0';
	job_ad->LookupString( ATTR_REMOVE_REASON, removeReason,
						   sizeof(removeReason) - 1 );

	event.setReason( removeReason );

	int rc = ulog->writeEvent(&event);
	delete ulog;

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
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
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
	if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
		if( int_val ) {
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

	float float_val = 0;
	if ( job_ad->LookupFloat( ATTR_JOB_REMOTE_USER_CPU, float_val ) ) {
		event.run_remote_rusage.ru_utime.tv_sec = (int)float_val;
		event.total_remote_rusage.ru_utime.tv_sec = (int)float_val;
	}
	if ( job_ad->LookupFloat( ATTR_JOB_REMOTE_SYS_CPU, float_val ) ) {
		event.run_remote_rusage.ru_stime.tv_sec = (int)float_val;
		event.total_remote_rusage.ru_stime.tv_sec = (int)float_val;
	}

	int rc = ulog->writeEvent(&event);
	delete ulog;

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
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
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

	int rc = ulog->writeEvent(&event);
	delete ulog;

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

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
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

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_HELD event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// The GlobusResourceUpEvent is now deprecated and should be removed at
// some point in the future (6.9?).
bool
WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus up record to user logfile\n",
			 cluster, proc );

	GlobusResourceUpEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	if ( contact.IsEmpty() ) {
			// Not a Globus job, don't log the event
		return true;
	}
	contact.Tokenize();
	contact.GetNextToken( " ", false );
	event.rmContact =  strnewp(contact.GetNextToken( " ", false ));

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_UP event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// The GlobusResourceDownEvent is now deprecated and should be removed at
// some point in the future (6.9?).
bool
WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus down record to user logfile\n",
			 cluster, proc );

	GlobusResourceDownEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	if ( contact.IsEmpty() ) {
			// Not a Globus job, don't log the event
		return true;
	}
	contact.Tokenize();
	contact.GetNextToken( " ", false );
	event.rmContact =  strnewp(contact.GetNextToken( " ", false ));

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_DOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// The GlobusSubmitEvent is now deprecated and should be removed at
// some point in the future (6.9?).
bool
WriteGlobusSubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	int version;
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus submit record to user logfile\n",
			 cluster, proc );

	GlobusSubmitEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	contact.Tokenize();
	contact.GetNextToken( " ", false );
	event.rmContact = strnewp(contact.GetNextToken( " ", false ));

	job_ad->LookupString( ATTR_GRID_JOB_ID, contact );
	contact.Tokenize();
	if ( strcasecmp( contact.GetNextToken( " ", false ), "gt2" ) == 0 ) {
		contact.GetNextToken( " ", false );
	}
	event.jmContact = strnewp(contact.GetNextToken( " ", false ));

	version = 0;
	job_ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, version );
	event.restartableJM = version >= GRAM_V_1_5;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad, int failure_code,
									   const char *failure_mesg )
{
	int cluster, proc;
	char buf[1024];

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing submit-failed record to user logfile\n",
			 cluster, proc );

	GlobusSubmitFailedEvent event;

	snprintf( buf, 1024, "%d %s", failure_code,
			  failure_mesg ? failure_mesg : "");
	event.reason =  strnewp(buf);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT_FAILED event\n",
				 cluster, proc);
		return false;
	}

	return true;
}

bool
WriteGridResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid resource up record to user logfile\n",
			 cluster, proc );

	GridResourceUpEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	if ( contact.IsEmpty() ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) %s attribute missing in job ad\n",
				 cluster, proc, ATTR_GRID_RESOURCE );
	}
	event.resourceName =  strnewp( contact.Value() );

	int rc = ulog->writeEvent( &event );
	delete ulog;

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
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid source down record to user logfile\n",
			 cluster, proc );

	GridResourceDownEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	if ( contact.IsEmpty() ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) %s attribute missing in job ad\n",
				 cluster, proc, ATTR_GRID_RESOURCE );
	}
	event.resourceName =  strnewp( contact.Value() );

	int rc = ulog->writeEvent(&event);
	delete ulog;

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
	int version;
	MyString contact;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing grid submit record to user logfile\n",
			 cluster, proc );

	GridSubmitEvent event;

	job_ad->LookupString( ATTR_GRID_RESOURCE, contact );
	event.resourceName = strnewp( contact.Value() );

	job_ad->LookupString( ATTR_GRID_JOB_ID, contact );
	event.jobId = strnewp( contact.Value() );

	int rc = ulog->writeEvent( &event );
	delete ulog;

	if ( !rc ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GRID_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// TODO: This appears three times in the Condor source.  Unify?
//   (It only is made visible in condor_shadow.jim's prototypes.h.)
static char *
d_format_time( double dsecs )
{
	int days, hours, minutes, secs;
	static char answer[25];

	const int SECONDS = 1;
	const int MINUTES = (60 * SECONDS);
	const int HOURS   = (60 * MINUTES);
	const int DAYS    = (24 * HOURS);

	secs = (int)dsecs;

	days = secs / DAYS;
	secs %= DAYS;

	hours = secs / HOURS;
	secs %= HOURS;

	minutes = secs / MINUTES;
	secs %= MINUTES;

	(void)sprintf(answer, "%3d %02d:%02d:%02d", days, hours, minutes, secs);

	return( answer );
}

void
EmailTerminateEvent(ClassAd * job_ad, bool exit_status_known)
{
	if ( !job_ad ) {
		dprintf(D_ALWAYS, 
				"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	int cluster, proc;
	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	int notification = NOTIFY_COMPLETE; // default
	job_ad->LookupInteger(ATTR_JOB_NOTIFICATION,notification);

	switch( notification ) {
		case NOTIFY_NEVER:    return;
		case NOTIFY_ALWAYS:   break;
		case NOTIFY_COMPLETE: break;
		case NOTIFY_ERROR:    return;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				cluster, proc, notification );
				// When in doubt, better send it anyway...
			break;
	}

	char subjectline[50];
	sprintf( subjectline, "Condor Job %d.%d", cluster, proc );
	FILE * mailer =  email_user_open( job_ad, subjectline );

	if( ! mailer ) {
		// Is message redundant?  Check email_user_open and euo's children.
		dprintf(D_ALWAYS, 
			"email_terminate_event failed to open a pipe to a mail program.\n");
		return;
	}

		// gather all the info out of the job ad which we want to 
		// put into the email message.
	char JobName[_POSIX_PATH_MAX];
	JobName[0] = '\0';
	job_ad->LookupString( ATTR_JOB_CMD, JobName );

	MyString Args;
	ArgList::GetArgsStringForDisplay(job_ad,&Args);
	
	/*
	// Not present.  Probably doesn't make sense for Globus
	int had_core = FALSE;
	job_ad->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
	*/

	int q_date = 0;
	job_ad->LookupInteger(ATTR_Q_DATE,q_date);
	
	float remote_sys_cpu = 0.0;
	job_ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);
	
	float remote_user_cpu = 0.0;
	job_ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);
	
	int image_size = 0;
	job_ad->LookupInteger(ATTR_IMAGE_SIZE, image_size);
	
	/*
	int shadow_bday = 0;
	job_ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	*/
	
	float previous_runs = 0;
	job_ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	fprintf( mailer, "Your Condor job %d.%d \n", cluster, proc);
	if ( JobName[0] ) {
		fprintf(mailer,"\t%s %s\n",JobName,Args.Value());
	}
	if(exit_status_known) {
		fprintf(mailer, "has ");

		int int_val;
		if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					fprintf(mailer, "exited with the signal %d.\n", int_val);
				} else {
					fprintf(mailer, "exited with an unknown signal.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				}
			} else {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					fprintf(mailer, "exited normally with status %d.\n",
						int_val);
				} else {
					fprintf(mailer, "exited normally with unknown status.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				}
			}
		} else {
			fprintf(mailer,"has exited.\n");
			dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  ",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		}
	} else {
		fprintf(mailer,"has exited.\n");
	}

	/*
	if( had_core ) {
		fprintf( mailer, "Core file is: %s\n", getCoreName() );
	}
	*/

	arch_time = q_date;
	fprintf(mailer, "\n\nSubmitted at:        %s", ctime(&arch_time));
	
	double real_time = now - q_date;
	arch_time = now;
	fprintf(mailer, "Completed at:        %s", ctime(&arch_time));
	
	fprintf(mailer, "Real Time:           %s\n", 
			d_format_time(real_time));


	fprintf( mailer, "\n" );
	
		// TODO We don't necessarily have this information even if we do
		//   have the exit status
	if( exit_status_known ) {
		fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n\n", image_size);
	}

	double rutime = remote_user_cpu;
	double rstime = remote_sys_cpu;
	double trtime = rutime + rstime;
	/*
	double wall_time = now - shadow_bday;
	fprintf(mailer, "Statistics from last run:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",d_format_time(wall_time) );
	*/
		// TODO We don't necessarily have this information even if we do
		//   have the exit status
	if( exit_status_known ) {
		fprintf(mailer, "Remote User CPU Time:    %s\n", d_format_time(rutime) );
		fprintf(mailer, "Remote System CPU Time:  %s\n", d_format_time(rstime) );
		fprintf(mailer, "Total Remote CPU Time:   %s\n\n", d_format_time(trtime));
	}

	/*
	double total_wall_time = previous_runs + wall_time;
	fprintf(mailer, "Statistics totaled from all runs:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",
			d_format_time(total_wall_time) );

	// TODO: Can we/should we get this for Globus jobs.
		// TODO: deal w/ total bytes <- obsolete? in original code)
	float network_bytes;
	network_bytes = bytesSent();
	fprintf(mailer, "\nNetwork:\n" );
	fprintf(mailer, "%10s Run Bytes Received By Job\n", 
			metric_units(network_bytes) );
	network_bytes = bytesReceived();
	fprintf(mailer, "%10s Run Bytes Sent By Job\n",
			metric_units(network_bytes) );
	*/

	email_close(mailer);
}
