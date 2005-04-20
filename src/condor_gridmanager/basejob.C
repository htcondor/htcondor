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
#include "environ.h"  // for Environ object
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"

#include "gridmanager.h"
#include "basejob.h"
#include "condor_config.h"
#include "condor_email.h"
#include "gridutil.h"


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

	ad = classad;

	ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	ad->LookupInteger( ATTR_PROC_ID, procID.proc );

	ad->LookupInteger( ATTR_JOB_STATUS, condorState );

	evaluateStateTid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&BaseJob::doEvaluateState,
								"doEvaluateState", (Service*) this );;

	wantRematch = 0;
	doResubmit = 0;		// set if gridmanager wants to resubmit job
	wantResubmit = 0;	// set if user wants to resubmit job via RESUBMIT_CHECK
	ad->EvalBool(ATTR_GLOBUS_RESUBMIT_CHECK,NULL,wantResubmit);

	ad->ClearAllDirtyFlags();

	periodicPolicyEvalTid = daemonCore->Register_Timer( 30,
								(TimerHandlercpp)&BaseJob::EvalPeriodicJobExpr,
								"EvalPeriodicJobExpr", (Service*) this );
}

BaseJob::~BaseJob()
{
	if ( ad ) {
		delete ad;
	}
	daemonCore->Cancel_Timer( periodicPolicyEvalTid );
	daemonCore->Cancel_Timer( evaluateStateTid );
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

void BaseJob::UpdateJobAd( const char *name, const char *value )
{
	UpdateClassAd(ad, name, value);
}

void BaseJob::UpdateJobAdInt( const char *name, int value )
{
	UpdateClassAdInt(ad, name, value);
}

void BaseJob::UpdateJobAdFloat( const char *name, float value )
{
	UpdateClassAdFloat(ad, name, value);
}

void BaseJob::UpdateJobAdBool( const char *name, int value )
{
	UpdateClassAdBool(ad, name, value);
}

void BaseJob::UpdateJobAdString( const char *name, const char *value )
{
	UpdateClassAdString(ad, name, value);
}

void BaseJob::JobSubmitted( const char *remote_host)
{
}

void BaseJob::JobRunning()
{
	if ( condorState != RUNNING && condorState != HELD &&
		 condorState != REMOVED ) {

		condorState = RUNNING;
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS, time(NULL) );

		UpdateRuntimeStats();

		if ( writeUserLog && !executeLogged ) {
			WriteExecuteEventToUserLog( ad );
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
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS, time(NULL) );

		UpdateRuntimeStats();

		requestScheddUpdate( this );
	}
}

void BaseJob::JobEvicted()
{
		// Does this imply a change to condorState IDLE?

	UpdateRuntimeStats();

		//  should we be updating job ad values here?
	if ( writeUserLog && !evictLogged ) {
		WriteEvictEventToUserLog( ad );
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
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS, time(NULL) );

		UpdateRuntimeStats();

		requestScheddUpdate( this );
	}
}

void BaseJob::DoneWithJob()
{
	deleteFromGridmanager = true;
	UpdateJobAdBool( ATTR_JOB_MANAGED, 0 );

	if ( condorState == COMPLETED ) {
		if ( writeUserLog && !terminateLogged ) {
			WriteTerminateEventToUserLog( ad );
			EmailTerminateEvent( ad, exitStatusKnown );
			terminateLogged = true;
		}
		deleteFromSchedd = true;
	}
	if ( condorState == REMOVED ) {
		if ( writeUserLog && !abortLogged ) {
			WriteAbortEventToUserLog( ad );
			abortLogged = true;
		}
		deleteFromSchedd = true;
	}
	if ( condorState == HELD ) {
		if ( writeUserLog && !holdLogged ) {
			WriteHoldEventToUserLog( ad );
			holdLogged = true;
		}
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
			UpdateJobAdInt( ATTR_JOB_STATUS_ON_RELEASE, REMOVED );
		}
		condorState = HELD;
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS, time(NULL) );

		UpdateJobAdString( ATTR_HOLD_REASON, hold_reason );
		UpdateJobAdInt(ATTR_HOLD_REASON_CODE, hold_code);
		UpdateJobAdInt(ATTR_HOLD_REASON_SUBCODE, hold_sub_code);

		char *release_reason;
		if ( ad->LookupString( ATTR_RELEASE_REASON, &release_reason ) != 0 ) {
			UpdateJobAdString( ATTR_LAST_RELEASE_REASON, release_reason );
			free( release_reason );
		}
		UpdateJobAd( ATTR_RELEASE_REASON, "UNDEFINED" );

		int num_holds;
		ad->LookupInteger( ATTR_NUM_SYSTEM_HOLDS, num_holds );
		num_holds++;
		UpdateJobAdInt( ATTR_NUM_SYSTEM_HOLDS, num_holds );

		UpdateRuntimeStats();

		if ( writeUserLog && !holdLogged ) {
			WriteHoldEventToUserLog( ad );
			holdLogged = true;
		}

		requestScheddUpdate( this );
	}
}

void BaseJob::JobRemoved( const char *remove_reason )
{
	if ( condorState != REMOVED ) {
		condorState = REMOVED;
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS, time(NULL) );

		UpdateJobAdString( ATTR_REMOVE_REASON, remove_reason );

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
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadowBirthdate );
	if ( condorState == RUNNING && shadowBirthdate == 0 ) {

		// The job has started a new interval of running
		int current_time = (int)time(NULL);
		UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, current_time );

		requestScheddUpdate( this );

	} else if ( condorState != RUNNING && shadowBirthdate != 0 ) {

		// The job has stopped an interval of running, add the current
		// interval to the accumulated total run time
		float accum_time = 0;
		ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		accum_time += (float)( time(NULL) - shadowBirthdate );
		UpdateJobAdFloat( ATTR_JOB_REMOTE_WALL_CLOCK, accum_time );
		UpdateJobAd( ATTR_JOB_WALL_CLOCK_CKPT, "UNDEFINED" );
		UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, 0 );

		requestScheddUpdate( this );

	}
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

	if ( new_condor_state == REMOVED || new_condor_state == HELD ) {

		for ( int i = 0; held_removed_update_attrs[i] != NULL; i++ ) {
			char attr_value[1024];
			ExprTree *expr;

			if ( (expr = new_ad->Lookup( held_removed_update_attrs[i] )) != NULL ) {
				attr_value[0] = '\0';
				expr->RArg()->PrintToStr(attr_value);
				UpdateJobAd( held_removed_update_attrs[i], attr_value );
			} else {
				ad->Delete( held_removed_update_attrs[i] );
			}
			ad->SetDirtyFlag( held_removed_update_attrs[i], false );
		}

		if ( new_condor_state == HELD && writeUserLog && !holdLogged ) {
			// TODO should this log event be delayed until gridmanager is
			//   done dealing with the job?
			WriteHoldEventToUserLog( ad );
			holdLogged = true;
		}

			// If we're about to put a job on hold and learn that it's been
			// removed, make sure the state returns to removed when it is
			// released. This is normally checked in JobHeld(), but it's
			// possible to learn of the removal just as we're about to
			// update the schedd with the hold.
		if ( new_condor_state == REMOVED && condorState == HELD ) {
			bool dirty;
			ad->GetDirtyFlag( ATTR_JOB_STATUS, NULL, &dirty );
			if ( dirty ) {
				UpdateJobAdInt( ATTR_JOB_STATUS_ON_RELEASE, REMOVED );
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

int BaseJob::EvalPeriodicJobExpr()
{
	float old_run_time;
	bool old_run_time_dirty;
	UserPolicy user_policy;

	user_policy.Init( ad );

	UpdateJobTime( &old_run_time, &old_run_time_dirty );

	int action = user_policy.AnalyzePolicy( PERIODIC_ONLY );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	switch( action ) {
	case UNDEFINED_EVAL:
		JobHeld( "Undefined job policy expression" );
		SetEvaluateState();
		break;
	case STAYS_IN_QUEUE:
			// do nothing
		break;
	case REMOVE_FROM_QUEUE:
		JobRemoved( "Remove job policy expression became true" );
		SetEvaluateState();
		break;
	case HOLD_IN_QUEUE:
		JobHeld( "Hold job policy expression became true" );
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

	daemonCore->Reset_Timer( periodicPolicyEvalTid, 30 );

	return 0;
}

int BaseJob::EvalOnExitJobExpr()
{
	float old_run_time;
	bool old_run_time_dirty;
	UserPolicy user_policy;

	user_policy.Init( ad );

	// The user policy code expects an exit value to be set
	// If the ON_EXIT attributes haven't been set at all, fake
	// a normal job exit.
	int dummy;
	if ( !ad->LookupInteger( ATTR_ON_EXIT_SIGNAL, dummy ) &&
		 !ad->LookupInteger( ATTR_ON_EXIT_CODE, dummy ) ) {

		exitStatusKnown = false;
		UpdateJobAdBool( ATTR_ON_EXIT_BY_SIGNAL, 0 );
		UpdateJobAdInt( ATTR_ON_EXIT_CODE, 0 );
	} else {
		exitStatusKnown = true;
	}

	// TODO: We should just mark the job as done running
	UpdateJobTime( &old_run_time, &old_run_time_dirty );

	int action = user_policy.AnalyzePolicy( PERIODIC_THEN_EXIT );

	RestoreJobTime( old_run_time, old_run_time_dirty );

	if ( action != REMOVE_FROM_QUEUE ) {
		UpdateJobAdBool( ATTR_ON_EXIT_BY_SIGNAL, 0 );
		UpdateJobAd( ATTR_ON_EXIT_CODE, "UNDEFINED" );
		UpdateJobAd( ATTR_ON_EXIT_SIGNAL, "UNDEFINED" );
	}

	switch( action ) {
	case UNDEFINED_EVAL:
		JobHeld( "Undefined job policy expression" );
		break;
	case STAYS_IN_QUEUE:
			// clean up job but don't set status to complete
		break;
	case REMOVE_FROM_QUEUE:
		JobCompleted();
		break;
	case HOLD_IN_QUEUE:
		JobHeld( "Hold job policy became true" );
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

  ad->LookupInteger(ATTR_SHADOW_BIRTHDATE,shadow_bday);
  ad->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,previous_run_time);
  ad->GetDirtyFlag(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,old_run_time_dirty);

  if (old_run_time) {
	  *old_run_time = previous_run_time;
  }
  total_run_time = previous_run_time;
  if (shadow_bday) {
	  total_run_time += (now - shadow_bday);
  }

  UpdateJobAdFloat( ATTR_JOB_REMOTE_WALL_CLOCK, total_run_time );
}

/*After evaluating user policy expressions, this is
  called to restore time values to their original state.
*/
void
BaseJob::RestoreJobTime( float old_run_time, bool old_run_time_dirty )
{
  UpdateJobAdFloat( ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time );
  ad->SetDirtyFlag( ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time_dirty );
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
	char *host_ptr;

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

		// TODO This formating assumes the hostname to be placed in the
		//   log event can be found in ATTR_GLOBUS_RESOURCE and is formatted
		//   like a gt2 or gt4 resource name. What about other job types?
	hostname[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, hostname,
						  sizeof(hostname) - 1 );
	if ( strncmp( "https://", hostname, 8 ) == 0 ) {
		host_ptr = &hostname[8];
	} else {
		host_ptr = hostname;
	}
	int hostname_len = strcspn( host_ptr, ":/" );

	ExecuteEvent event;
	strncpy( event.executeHost, host_ptr, hostname_len );
	event.executeHost[hostname_len] = '\0';
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
EmailTerminateEvent(ClassAd * jobAd, bool exit_status_known)
{
	if ( !jobAd ) {
		dprintf(D_ALWAYS, 
				"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	int cluster, proc;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, proc );

	int notification = NOTIFY_COMPLETE; // default
	jobAd->LookupInteger(ATTR_JOB_NOTIFICATION,notification);

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
	FILE * mailer =  email_user_open( jobAd, subjectline );

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
	jobAd->LookupString( ATTR_JOB_CMD, JobName );

	char Args[_POSIX_ARG_MAX];
	Args[0] = '\0';
	jobAd->LookupString(ATTR_JOB_ARGUMENTS, Args);
	
	/*
	// Not present.  Probably doesn't make sense for Globus
	int had_core = FALSE;
	jobAd->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
	*/

	int q_date = 0;
	jobAd->LookupInteger(ATTR_Q_DATE,q_date);
	
	float remote_sys_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);
	
	float remote_user_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);
	
	int image_size = 0;
	jobAd->LookupInteger(ATTR_IMAGE_SIZE, image_size);
	
	/*
	int shadow_bday = 0;
	jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	*/
	
	float previous_runs = 0;
	jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	fprintf( mailer, "Your Condor job %d.%d \n", cluster, proc);
	if ( JobName[0] ) {
		fprintf(mailer,"\t%s %s\n",JobName,Args);
	}
	if(exit_status_known) {
		fprintf(mailer, "has ");

		int int_val;
		if( jobAd->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					fprintf(mailer, "exited with the signal %d.\n", int_val);
				} else {
					fprintf(mailer, "exited with an unknown signal.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				}
			} else {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
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
