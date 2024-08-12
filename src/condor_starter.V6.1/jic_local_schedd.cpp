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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "condor_classad.h"
#include "exit.h"
#include "internet.h"

#include "starter.h"
#include "starter_user_policy.h"
#include "jic_local.h"
#include "jic_local_schedd.h"
#include "condor_version.h"

extern class Starter *starter;


JICLocalSchedd::JICLocalSchedd( const char* classad_filename,
								const char* schedd_address, 
								int cluster, int proc, int subproc )
	: JICLocalFile( classad_filename, cluster, proc, subproc )
{
		// initialize this to something reasonable.  we'll change it
		// if anything special happens which needs a different value.
	exit_code = JOB_EXITED;
	if( ! is_valid_sinful(schedd_address) ) {
        EXCEPT("schedd_addr not specified with valid address");
    }
    schedd_addr = strdup( schedd_address );
	dprintf( D_ALWAYS,
			 "Starter running a job under a schedd listening at %s\n",
			 schedd_addr );

	job_updater = NULL;
	m_cleanup_retry_tid = -1;
	m_num_cleanup_retries = 0;
		// NOTE: these config knobs are very similar to
		// SHADOW_MAX_JOB_CLEANUP_RETRIES and
		// SHADOW_JOB_CLEANUP_RETRY_DELAY in the shadow.
	m_max_cleanup_retries = param_integer("LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES", 5);
	m_cleanup_retry_delay = param_integer("LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY", 30);
	this->starter_user_policy = NULL;
	m_tried_notify_job_exit = false;
}


JICLocalSchedd::~JICLocalSchedd()
{
	if( schedd_addr ) {
		free( schedd_addr );
	}
	if( job_updater ) {
		delete job_updater;
	}
	if( m_cleanup_retry_tid >= 0 ) {
		daemonCore->Cancel_Timer(m_cleanup_retry_tid);
		m_cleanup_retry_tid = -1;
	}
	if ( this->starter_user_policy ) {
		delete this->starter_user_policy;
	}
}

/**
 * This will call JICLocal::init() and setup all that
 * we need for the job info communicator. If the init()
 * was successfull and we have a job ad, we will instantiate
 * and fire up the StarterUserPolicy object so that
 * local universe jobs now have periodic expressions
 * 
 * @return true if the initialization was successful
 */ 
bool
JICLocalSchedd::init( void )
{
	bool ret = JICLocal::init( );
	
		//
		// JICLocal now has a UserPolicy object to do policy
		// expression evaluation. We need to give it the job ad
		// and a reference back to ourself so that we can do the
		// appropriate action
		//
	if ( ret && this->job_ad ) {
		this->starter_user_policy = new StarterUserPolicy( );
		this->starter_user_policy->init( this->job_ad, this );
		this->starter_user_policy->startTimer( );
	}
	return ( ret );
}

/**
 * Puts the job on hold. The Starter actually does the 
 * dirty work, we just add the reason to the ad and email the
 * user. We set the exit_code to JOB_SHOULD_HOLD so that
 * schedd will put the job on hold in the queue. We do NOT need
 * to write an EVICT log event or update the job queue, because
 * JICLocalSchedd::notifyJobExit() will take care of that for us.
 * 
 * @param reason - why the job is going on hold
 * @return true if the jobs were told to be put on hold
 */
bool
JICLocalSchedd::holdJob( const char *reason, int hold_reason_code, int hold_reason_subcode ) {
		//
		// First tell the Starter to put the job on hold
		// This method will not call gotHold() back on us,
		// so we have to do it explicitly.
		//
	starter->Hold( );
	gotHold( );
		//
		// Insert the reason into the job ad 
		//
	this->job_ad->Assign( ATTR_HOLD_REASON, reason );
	this->job_ad->Assign( ATTR_HOLD_REASON_CODE, hold_reason_code );
	this->job_ad->Assign( ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode );
	return true;
}

/**
 * The job needs to be removed from the Starter. This
 * is more than just calling starter->Remove() because we need
 * to stuff the remove reason in the job ad and update the job queue.
 * We also set the proper exit code for the Starter. We do NOT need
 * to write an EVICT log event or update the job queue, because
 * JICLocalSchedd::notifyJobExit() will take care of that for us.
 * 
 * @param reason - why the job is being removed
 * @return true if the job was set to be removed
 **/
bool
JICLocalSchedd::removeJob( const char *reason ) {
		//
		// First tell the Starter to put the job on hold
		// This method will not call gotRemove() back on us,
		// so we have to do it explicitly.
		//
	starter->Remove( );
	gotRemove();
		//
		// Insert the reason into the job ad 
		//
	this->job_ad->Assign( ATTR_REMOVE_REASON, reason );
	return true;
}

/**
 * The job exited normally.  Nothing to do.
 * 
 * @param reason - how the job exited
 * @return true
 **/
bool
JICLocalSchedd::terminateJob( const char* )
{
	return true;
}

/**
 * The job needs to be requeued back on the schedd, so 
 * we need to update the job ad and set
 * the proper exit code for the Starter so that the schedd
 * knows to put the job back into idle state.
 * 
 * @param reason - why the job is being requeued
 * @return true if the job was set to be requeued
 **/
bool
JICLocalSchedd::requeueJob( const char *reason ) {
		//
		// Insert the reason into the job ad 
		//
	this->job_ad->Assign( ATTR_REQUEUE_REASON, reason );
		//
		// We set the proper exit_code for the schedd
		// to do the action that we want it to
		//
	this->exit_code = JOB_SHOULD_REQUEUE;
	starter->Remove();
	return true;
}

void
JICLocalSchedd::allJobsGone( void )
{
	if( !m_tried_notify_job_exit && this->exit_code == JOB_SHOULD_HOLD ) {
			// The job startup must have failed.
			// Notify the schedd of the hold reason etc.
		notifyJobExit(this->exit_code,JOB_KILLED,NULL);
	}
		// Since there's no shadow to tell us to go away, we have to
		// exit ourselves.  However, we need to use the right code so
		// the schedd knows to remove the job from the queue
	dprintf( D_ALWAYS, "All jobs have exited... starter exiting\n" );
	starter->StarterExit( exit_code );
}

void
JICLocalSchedd::setExitCodeToRequeue()
{
	switch(exit_code) {
	case JOB_SHOULD_HOLD:
	case JOB_SHOULD_REMOVE:
	case JOB_MISSED_DEFERRAL_TIME:
			// Ignore JOB_SHOULD_REQUEUE, because we are already set
			// to do something more drastic.
		break;
	default:
		exit_code = JOB_SHOULD_REQUEUE;
	}
}

void
JICLocalSchedd::gotShutdownFast( void )
{
	JobInfoCommunicator::gotShutdownFast();
	setExitCodeToRequeue();
}


void
JICLocalSchedd::gotShutdownGraceful( void )
{
	JobInfoCommunicator::gotShutdownGraceful();
	setExitCodeToRequeue();
}


void
JICLocalSchedd::gotRemove( void )
{
	JobInfoCommunicator::gotRemove();
	this->exit_code = JOB_SHOULD_REMOVE;
}


void
JICLocalSchedd::gotHold( void )
{
	JobInfoCommunicator::gotHold();
	this->exit_code = JOB_SHOULD_HOLD;
}


bool
JICLocalSchedd::getLocalJobAd( void )
{ 
	if( ! JICLocalFile::getLocalJobAd() ) {
		return false;
	}
	job_updater = new QmgrJobUpdater( job_ad, schedd_addr );
	return true;
}

void
JICLocalSchedd::notifyJobPreSpawn( void )
{
	JICLocal::notifyJobPreSpawn();

    int job_start_cnt = 0;
    job_ad->LookupInteger(ATTR_NUM_JOB_STARTS, job_start_cnt);
    job_start_cnt++;
    job_updater->updateAttr(ATTR_NUM_JOB_STARTS, job_start_cnt, false);
}

/**
 * A job is exiting the Starter and we need to take necessary
 * actions. First we will update the job's ad file with various
 * information about what the job did. Next, if the job completed on
 * its own, we'll want to call the StarterUserPolicy's checkAtExit(),
 * which handles setting the right exit status to control the job's
 * final state in the job queue. If the job is being killed from "unnatural"
 * causes, such as a condor_rm, then we will figure out the right
 * update type is for the job and write an EVICT event to the user log.
 * 
 * @param exit_status - the exit status of the job from wait()
 * This is not used currently
 * @param reason - the Condor-defined reason why the job is exiting
 * @param user_proc - the Proc object for this job
 * @return true if the job was set to exit properly
 * @see h/exit.h
 **/
bool
JICLocalSchedd::notifyJobExit( int, int reason, 
							   UserProc* user_proc )
{
		// Remember what steps we've completed, in case we need to retry.
	static bool did_final_ad_publish = false;
	static bool did_schedd_update = false;
	static bool did_check_at_exit = false;
	static bool did_ulog_event = false;

	m_tried_notify_job_exit = true;
 
	if (!did_final_ad_publish) {
			// Prepare to update the job queue.  In this case, we want
			// to publish all the same attribute we'd otherwise send
			// to the shadow, but instead, just stick them directly
			// into our copy of the job classad.
		starter->publishPreScriptUpdateAd( job_ad );
		if( user_proc ) {
			user_proc->PublishUpdateAd( job_ad );
		}
		starter->publishPostScriptUpdateAd( job_ad );
		did_final_ad_publish = true;
	}
	
		// Only check to see what we should do with our job 
		// in the user policy object if the job terminated
		// on its own.  Otherwise, we've already been there
		// and done that.
	if ( reason == JOB_EXITED || reason == JOB_COREDUMPED ) {
		if( !did_check_at_exit ) {
				// What should be the return value for this?
				// Can I just assume that things went well?
			this->starter_user_policy->checkAtExit( );
			did_check_at_exit = true;
		}
	}
	else if( reason == JOB_MISSED_DEFERRAL_TIME ) {
			//
			// This is suppose to be temporary until we have some kind
			// of error handling in place for jobs that never started
			// Andy Pavlo - 01.24.2006 - pavlo@cs.wisc.edu
			//
		exit_code = JOB_MISSED_DEFERRAL_TIME;
	}

	if( !did_ulog_event ) {
			// Use the final exit code to determine what event to log.
			// This may be different from what is indicated by 'reason',
			// because a policy expression evaluted by checkAtExit() may
			// have changed things.
		switch( this->exit_code ) {
		case JOB_EXITED:
			this->u_log->logTerminate( this->job_ad );
			did_ulog_event = true;
			break;
		case JOB_SHOULD_REQUEUE:
			// Following the baseshadow, if the job is being requeued
			// then it is an eviction event
			this->u_log->logRequeueEvent( this->job_ad, false );
			did_ulog_event = true;
			break;
		case JOB_SHOULD_REMOVE:
		case JOB_SHOULD_HOLD:
		case JOB_MISSED_DEFERRAL_TIME:
			// NOTE: The local universe's log actions are not consistent
			// with what the Shadow does. This is because the Shadow is
			// not consistent with itself; for example, a condor_rm
			// will cause an EVICT notice in the user log, but a 
			// periodic remove will not. This is something Derek
			// said he will clean up later on. For now, however, we are
			// going to be consistent with ourself in the local universe
			// and ALWAYS send an eviction notice when the job is 
			// removed
			this->u_log->logEvict( this->job_ad, false );
			did_ulog_event = true;
			break;
		default:
			EXCEPT("Internal error in JICLocalSchedd::notifyJobExit: unexpected exit code %d",this->exit_code);
		}
	}


	if( !did_schedd_update ) {
			// Use the final exit code to determine the update type.
			// This may be different from what is indicated by 'reason',
			// because a policy expression evaluted by checkAtExit() may
			// have changed things.
		update_t up_type = U_TERMINATE;
		switch( this->exit_code ) {
		case JOB_EXITED:
			up_type = U_TERMINATE;
			break;
		case JOB_SHOULD_REQUEUE:
			up_type = U_REQUEUE;
			break;
		case JOB_SHOULD_REMOVE:
			up_type = U_REMOVE;
			break;
		case JOB_SHOULD_HOLD:
		case JOB_MISSED_DEFERRAL_TIME:
			up_type = U_HOLD;
			break;
		default:
			EXCEPT("Internal error in JICLocalSchedd::notifyJobExit: unexpected exit code %d",this->exit_code);
		}

			// Now that we've logged the event, we can update the job queue
			// If we're doing a fast shutdown, don't retry on failure.
		if ( !this->job_updater->updateJob( up_type ) && !fast_exit ) {
			dprintf( D_ALWAYS,
			         "Failed to update job queue - attempting to retry.\n" );
			retryJobCleanup();
			return ( false );
		}

		did_schedd_update = true;
	}

		//
		// Once the job's been updated in the queue, we can also try
		// sending email notification, if desired.
		// This returns void, so there's no way to test for failure.
		// Therefore, we don't bother with retry.
		//
	Email msg;
	switch( this->exit_code ) {
	case JOB_SHOULD_REQUEUE:
	case JOB_EXITED:
		msg.sendExit( job_ad, reason );
		break;
	case JOB_SHOULD_REMOVE: {
		char *remove_reason = NULL;
		this->job_ad->LookupString( ATTR_REMOVE_REASON, &remove_reason );
		msg.sendRemove( this->job_ad, remove_reason ? remove_reason : "" );
		free( remove_reason );
		break;
	}
	case JOB_SHOULD_HOLD: {
		char *hold_reason = NULL;
		this->job_ad->LookupString( ATTR_HOLD_REASON, &hold_reason );
		msg.sendHold( this->job_ad, hold_reason ? hold_reason : "" );
		free( hold_reason );
		break;
	}
	case JOB_MISSED_DEFERRAL_TIME:
		msg.sendHold( this->job_ad, "missed derreral time" );
		break;
	default:
		EXCEPT("Internal error in JICLocalSchedd::notifyJobExit: unexpected exit code %d",this->exit_code);
	}

		//
		// Lastly, we will call to write out the file. This was 
		// originally done in JICLocal::notifyJobExit(), but we no
		// longer call that
		//
	this->writeOutputAdFile( this->job_ad );

		//
		// Once we get here, everything has been successfully
		// wrapped up.
		//
	return true;
}


bool
JICLocalSchedd::getUniverse( void )
{
	int univ;

		// first, see if we've already got it in our ad
	if( ! job_ad->LookupInteger(ATTR_JOB_UNIVERSE, univ) ) {
		dprintf( D_ALWAYS, "\"%s\" not found in job ClassAd\n", 
				 ATTR_JOB_UNIVERSE );
		return false;
	}

	if( univ != CONDOR_UNIVERSE_LOCAL ) {
		dprintf( D_ALWAYS,
				 "ERROR: %s %s (%d) is not supported by JICLocalSchedd\n", 
				 ATTR_JOB_UNIVERSE, CondorUniverseName(univ), univ );
		return false;
	}
	
	return true;
}


bool
JICLocalSchedd::initLocalUserLog( void )
{
	bool ret = u_log->initFromJobAd( job_ad, false );
	if( ! ret ) {
		job_ad->Assign( ATTR_HOLD_REASON, "Failed to initialize user log");
		job_ad->Assign( ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE::UnableToInitUserLog );
		job_ad->Assign( ATTR_HOLD_REASON_SUBCODE, 0 );
		job_updater->updateJob(U_HOLD);
		starter->StarterExit(JOB_SHOULD_HOLD);
	}
	return true;
}


void
JICLocalSchedd::retryJobCleanup( void )
{
    m_num_cleanup_retries++;
	if (m_num_cleanup_retries > m_max_cleanup_retries) {
		EXCEPT("Maximum number of job cleanup retry attempts "
		       "(LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES=%d) reached",
		       m_max_cleanup_retries);
	}
	if( m_cleanup_retry_tid != -1 ) {
			// We can get here if, for example, we already failed once
			// to update the job queue and now the job has been
			// removed with condor_rm, forcing a new attempt.
		dprintf( D_ALWAYS, "Job cleanup already pending.\n" );
		return;
	}
	m_cleanup_retry_tid = daemonCore->Register_Timer(m_cleanup_retry_delay, 0,
					(TimerHandlercpp)&JICLocalSchedd::retryJobCleanupHandler,
					"retry job cleanup", this);
	dprintf(D_FULLDEBUG, "Will retry job cleanup in "
	        "LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY=%d seconds\n",
	        m_cleanup_retry_delay);
}


void
JICLocalSchedd::retryJobCleanupHandler( int /* timerID */ )
{
    m_cleanup_retry_tid = -1;
    dprintf(D_ALWAYS, "Retrying job cleanup, calling allJobsDone()\n");
    starter->allJobsDone();
}

bool
JICLocalSchedd::notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode )
{
	JICLocal::notifyStarterError(err_msg,critical,hold_reason_code,hold_reason_subcode);
	if( hold_reason_code ) {
		holdJob( err_msg, hold_reason_code, hold_reason_subcode );
	}
	return true;
}
