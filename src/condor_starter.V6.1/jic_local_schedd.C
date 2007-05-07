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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "condor_classad_util.h"
#include "exit.h"
#include "internet.h"

#include "starter.h"
#include "starter_user_policy.h"
#include "jic_local.h"
#include "jic_local_schedd.h"

extern CStarter *Starter;


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
	m_max_cleanup_retries = param_integer("LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES", 5);
	m_cleanup_retry_delay = param_integer("LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY", 30);
	this->starter_user_policy = NULL;
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
 * to write an EVICT log event because JICLocalSchedd::notifyJobExit()
 * will take care of that for us.
 * 
 * @param reason - why the job is going on hold
 * @return true if the jobs were told to be put on hold
 */
bool
JICLocalSchedd::holdJob( const char *reason ) {
	bool ret = false;
		//
		// First tell the Starter to put the job on hold
		// This method will not call gotHold() back on us
		//
		// Do I care what the return result is?
		//
	Starter->Hold( );
		
		//
		// Insert the reason into the job ad 
		//
	InsertIntoAd( this->job_ad, ATTR_HOLD_REASON, reason );
	
		//
		// Email the user that their job was put on hold
		// Do I need to do this? Does it even work?
		//
	Email mailer;
	mailer.sendHold( this->job_ad, reason );
	
		//
		// Call the queue manager to update the job ad
		//
		// What do I do if the update fails?
		//
	if ( this->job_updater->updateJob( U_HOLD ) ) {	
			//
			// Lastly, we set the proper exit_code for the schedd
			// to do the action that we want it to
			//
		this->exit_code = JOB_SHOULD_HOLD;
		ret = true;
	}
	return ( ret );
}

/**
 * The job needs to be removed from the Starter. This
 * is more than just calling Starter->Remove() because we need
 * to stuff the remove reason in the job ad and update the job queue.
 * We also set the proper exit code for the Starter. We do NOT need
 * to write an EVICT log event because JICLocalSchedd::notifyJobExit()
 * will take care of that for us.
 * 
 * @param reason - why the job is being removed
 * @return true if the job was set to be removed
 **/
bool
JICLocalSchedd::removeJob( const char *reason ) {
	bool ret = false;
		//
		// First tell the Starter to put the job on hold
		// This method will not call gotHold() back on us
		//
		// Do I care what the return result is?
		//
	Starter->Remove( );
		
		//
		// Insert the reason into the job ad 
		//
	InsertIntoAd( this->job_ad, ATTR_REMOVE_REASON, reason );
	
		//
		// Email the user that their job was removed
		//
	Email mailer;
	mailer.sendRemove( this->job_ad, reason );
	
		//
		// Call the queue manager to update the job ad
		//
		// What do I do if the update fails?
		//		
	if ( this->job_updater->updateJob( U_REMOVE ) ) {	
			//
			// Lastly, we set the proper exit_code for the schedd
			// to do the action that we want it to
			//
		this->exit_code = JOB_SHOULD_REMOVE;
		ret = true;
	}
	return ( ret );
}

/*
 * The job exited on its own accord and its not to be requeued,
 * so we need to update the job and write a TERMINATE event 
 * into the user log. It is important that we do NOT set the
 * exit_code in here like we do in the other methods.
 * 
 * @param reason - why the job is being terminated
 * @return true if the job was set to be terminated
 **/
bool
JICLocalSchedd::terminateJob( const char *reason ) {
	bool ret = false;
		//
		// Do I care if the updateJob() fails?
		//
	this->job_updater->updateJob( U_TERMINATE );
	ret = this->u_log->logTerminate( this->job_ad );
	return ( ret );
}

/**
 * The job needs to be requeued back on the schedd, so 
 * we need to update the job ad, the queue, and write 
 * a REQUEUE event into the user log. We have to set
 * the proper exit code for the Starter so that the schedd
 * knows to put the job back on hold.
 * 
 * @param reason - why the job is being requeued
 * @return true if the job was set to be requeued
 **/
bool
JICLocalSchedd::requeueJob( const char *reason ) {
	bool ret = false;
		
		//
		// Insert the reason into the job ad 
		//
	InsertIntoAd( this->job_ad, ATTR_REQUEUE_REASON, reason );
		
		//
		// Call the queue manager to update the job ad
		//
		// What do I do if the update fails?
		//
	if ( this->job_updater->updateJob( U_REQUEUE ) ) {	
			//
			// We set the proper exit_code for the schedd
			// to do the action that we want it to
			//
		this->exit_code = JOB_SHOULD_REQUEUE;
			//
			// We need to the tell the user log to write
			// the event. Following the baseshadow, if the
			// job is being requeued then it is an eviction event
			//
			// Should I always be saying the job WAS NOT checkpointed? 
			//
		ret = this->u_log->logRequeueEvent( this->job_ad, false );			
	}
	return ( ret );
}

void
JICLocalSchedd::allJobsGone( void )
{
		// Since there's no shadow to tell us to go away, we have to
		// exit ourselves.  However, we need to use the right code so
		// the schedd knows to remove the job from the queue
	dprintf( D_ALWAYS, "All jobs have exited... starter exiting\n" );
	Starter->StarterExit( exit_code );
}


void
JICLocalSchedd::gotShutdownFast( void )
{
	JobInfoCommunicator::gotShutdownFast();
	exit_code = JOB_SHOULD_REQUEUE;

}


void
JICLocalSchedd::gotShutdownGraceful( void )
{
	JobInfoCommunicator::gotShutdownGraceful();
	exit_code = JOB_SHOULD_REQUEUE;
}


void
JICLocalSchedd::gotRemove( void )
{
	JobInfoCommunicator::gotRemove();
	exit_code = JOB_KILLED;
}


void
JICLocalSchedd::gotHold( void )
{
	JobInfoCommunicator::gotHold();
	exit_code = JOB_KILLED;
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


/**
 * A job is exiting the Starter and we need to take necessary
 * actions. First we will update the job's ad file with various
 * information about what the job did. Next, if the job completed on
 * its own, we'll want to call the StarterUserPolicy's checkAtExit(),
 * which handles writing any user log events and updating the job
 * queue back on the schedd. If the job is being killed from "unnatural"
 * causes, such as a condor_rm, then we will figure out the right
 * update type is for the job and write an EVICT event to the user log.
 * 
 * @param exit_status - the exit status of the job from wait()
 * @param reason - the Condor-defined reason why the job is exiting
 * @param user_proc - the Proc object for this job
 * @return true if the job was set to exit properly
 * @see h/exit.h
 **/
bool
JICLocalSchedd::notifyJobExit( int exit_status, int reason, 
							   UserProc* user_proc )
{
		// Remember what steps we've completed, in case we need to retry.
	static bool did_local_notify = false;
	static bool did_final_ad_publish = false;
	static bool did_schedd_update = false;
	bool rval = true;

		// Call our parent's version of this method to handle all the
		// common-case stuff, like writing to the local userlog,
		// writing an output classad (if desired, etc).  
	if (!did_local_notify) {
		//
		// We used to call JICLocal::notifyJobExit() here but 
		// it interferes with our new policy expressions because
		// it doesn't allow us to fine tune the log event
		// Everything that this method used to do for us has been
		// broken out and tailored to do the different actions
		// that we may need to do using policy expressions. Thus,
		// it has been commented out and should remain so
		//
		//if (!JICLocal::notifyJobExit(exit_status, reason, user_proc)) {
		//	dprintf( D_ALWAYS, "JICLocal::notifyJobExit() failed - "
		//			 "scheduling retry\n" );
		//	retryJobCleanup();
		//	return false;
		//}
		did_local_notify = true;
	}

	if (!did_final_ad_publish) {
			// Prepare to update the job queue.  In this case, we want
			// to publish all the same attribute we'd otherwise send
			// to the shadow, but instead, just stick them directly
			// into our copy of the job classad.
		Starter->publishPreScriptUpdateAd( job_ad );
		user_proc->PublishUpdateAd( job_ad );
		Starter->publishPostScriptUpdateAd( job_ad );
		did_final_ad_publish = true;
	}
	
		// Only check to see what we should do with our job 
		// in the user policy object if the job was terminated
	if ( reason == JOB_EXITED || reason == JOB_COREDUMPED ) {
			// What should be the return value for this?
			// Can I just assume that things went well?
		this->starter_user_policy->checkAtExit( );
		
	} else if (!did_schedd_update) {
		// We need to send an eviction notice. We have to update
		// the job ad based on how we got here
		update_t up_type;
		switch( reason ) {
			case JOB_NOT_CKPTED:
				up_type = U_EVICT;
				break;
				
			case JOB_COREDUMPED:
			case JOB_EXITED:
				up_type = U_TERMINATE;
				break;

			case JOB_KILLED:
				if ( this->had_remove ) {
					up_type = U_REMOVE;
				} else if ( this->had_hold ) {
					up_type = U_HOLD;
				} else {
					EXCEPT( "Impossible: exit reason is JOB_KILLED, but "
							"neither had_hold or had_remove are TRUE!" );
				}
				break;

			case JOB_MISSED_DEFERRAL_TIME:
					//
					// This is suppose to be temporary until we have some kind
					// of error handling in place for jobs that never started
					// Andy Pavlo - 01.24.2006 - pavlo@cs.wisc.edu
					//
				up_type = U_HOLD;
					//
					// Set the exit code to be the deferral exit code
					//
				exit_code = JOB_MISSED_DEFERRAL_TIME;
				break;

			default:
				EXCEPT( "Programmer error: unknown reason (%d) in "
						"JICLocalSchedd::notifyJobExit", reason );
				break;
		} // SWITCH
		
			// Write an eviction notice
			// We need to pass whether the job was checkpointed or not
			// 
			// NOTE: The local universe's log actions is not consistent
			// with what the Shadow does. This is because the Shadow is
			// not consistent with itself; for example, a condor_rm
			// will cause an EVICT notice in the user log, but a 
			// periodic remove will not. This is something Derek
			// said he will clean up later on. For now, however, we are
			// going to be consistent with ourself in the local universe
			// and ALWAYS send an eviction notice when the job is 
			// removed
		rval = this->u_log->logEvict( this->job_ad, (reason == JOB_CKPTED) );

			// Now that we've logged the event, we can update the job queue
		this->job_updater->updateJob( up_type );
		
		did_schedd_update = true;
	}

		//
		// Once the job's been updated in the queue, we can also try
		// sending email notification, if desired.
		// This returns void, so there's no way to test for failure.
		// Therefore, we don't bother with retry.
		//
	Email msg;
	msg.sendExit( job_ad, reason );

		//
		// Lastly, we will call to write out the file. This was 
		// originally done in JICLocal::notifyJobExit(), but we no
		// longer need to call that
		// Does this need to happen before I do anything else? 
		// I'm thinking no
		//
	this->writeOutputAdFile( this->job_ad );

		//
		// The return value may need to be looked at again
		// to reflect what really happended up above
		//
	return ( rval );
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
	return u_log->initFromJobAd( job_ad, ATTR_ULOG_FILE,
								 ATTR_ULOG_USE_XML );
}


void
JICLocalSchedd::retryJobCleanup( void )
{
    m_num_cleanup_retries++;
	if (m_num_cleanup_retries > m_max_cleanup_retries) {
		EXCEPT("Maximum number of job cleanup retry attempts (%d) reached",
				m_max_cleanup_retries);
	}
	ASSERT(m_cleanup_retry_tid == -1);
	m_cleanup_retry_tid = daemonCore->Register_Timer(m_cleanup_retry_delay, 0,
					(TimerHandlercpp)&JICLocalSchedd::retryJobCleanupHandler,
					"retry job cleanup", this);
	dprintf(D_FULLDEBUG, "Will retry job cleanup in %d seconds\n",
			 m_cleanup_retry_delay);
}


int
JICLocalSchedd::retryJobCleanupHandler( void )
{
    m_cleanup_retry_tid = -1;
    dprintf(D_ALWAYS, "Retrying job cleanup, calling allJobsDone()\n");
    Starter->allJobsDone();
    return TRUE;
}
