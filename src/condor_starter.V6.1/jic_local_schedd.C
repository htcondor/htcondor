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
#include "exit.h"
#include "internet.h"

#include "starter.h"
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


bool
JICLocalSchedd::notifyJobExit( int exit_status, int reason, 
							   UserProc* user_proc )
{

		// Remember what steps we've completed, in case we need to retry.
	static bool did_local_notify = false;
	static bool did_final_ad_publish = false;
	static bool did_schedd_update = false;

		// Call our parent's version of this method to handle all the
		// common-case stuff, like writing to the local userlog,
		// writing an output classad (if desired, etc).  
	if (!did_local_notify) {
		if (!JICLocal::notifyJobExit(exit_status, reason, user_proc)) {
			dprintf( D_ALWAYS, "JICLocal::notifyJobExit() failed - "
					 "scheduling retry\n" );
			retryJobCleanup();
			return false;
		}
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

	if (!did_schedd_update) {
			// Once all the attributes have been published into our
			// copy of the job classad, we can just tell our updater
			// object to do the rest...
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
			if( had_remove ) {
				up_type = U_REMOVE;
			} else if ( had_hold ) {
				up_type = U_HOLD;
			} else {
				EXCEPT( "Impossible: exit reason is JOB_KILLED, but "
						"neither had_hold or had_remove are TRUE!" );
			}
			break;
		case JOB_MISSED_DEFERRAL_TIME:
			up_type = U_REMOVE;
				//
				// Set the exit code to be the deferral exit code
				//
			exit_code = JOB_MISSED_DEFERRAL_TIME;
			break;
		default:
			EXCEPT( "Programmer error: unknown reason (%d) in "
					"JICLocalSchedd::notifyJobExit", reason );
			break;
		}
		if (!job_updater->updateJob(up_type)) {
			dprintf( D_ALWAYS,
					 "Failed to update job queue - attempting to retry.\n" );
			retryJobCleanup();
			return false;
		}
		did_schedd_update = true;
	}

		// once the job's been updated in the queue, we can also try
		// sending email notification, if desired.
		// This returns void, so there's no way to test for failure.
		// Therefore, we don't bother with retry.
	Email msg;
	msg.sendExit( job_ad, reason );

		// If we got this far, we're sure we completed everything
		// successfully, so return success.
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
